#include "platform.h"

#if USING(OS_WINDOWS)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#endif //#if USING(OS_WINDOWS)

#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../scheduler/task.h"

#include "fiber.h"

#include <thread>
#include <unordered_map>
#include <type_traits>
#include <atomic>
#include <optional>
#include <algorithm>

/* Basic approach is to try to only use SPSC queues, which, with work stealing,
 * is a bit complex. Whenever any task creates a new task, the new task is added
 * to the active task's thread's unassignedTasks queue.
 * Every thread tries to exhaust it's runningTasks queue first (to minimize the
 * number of active fibers), then it's tasksAwaitingExecution queue, which
 * is bounded and somewhat small. If both queues are empty, the thread locks
 * the scheduler for work stealing (if the scheduler is aleady locked the thread
 * goes to sleep until it has tasks to run again) and goes through all other threads
 * moving ReactorThread finishedTasks to their appropriate thread's runningTasks
 * queue; moving Thread stalledTasks to their approrpiate reactor thread's
 * runningTasks queue or back to the same threads runningTasks queue (in the
 * event of a yield); and unassignedTasks to fill up all threads' 
 * tasksAwaitingExecution queues. Since only one thread can be running the
 * work stealing operation at a time, all queues can be spsc, only caveat
 * is the THRED_WAIT_QUEUE_SIZE.
 */

namespace
{
	static constexpr unsigned THREAD_WAIT_QUEUE_SIZE_LG2 = 4;

	struct Task
	{
		void (*TaskFunc)(void*);
		struct
		{
			uintptr_t userDataPtr : sizeof(uintptr_t) * 8 - 1;
			uintptr_t ownedPtr : 1;
		};
	};

	struct ScheduledFiber
	{
		fiber::Fiber* fiber;
		unsigned threadId;
		uint8_t _padding[4];
	};

	namespace spsc
	{
		namespace lg2
		{
			static constexpr unsigned RoundUp(unsigned v)
			{
				unsigned cur = v;
				unsigned lg2 = 0;

				while (cur >>= 1)
				{
					++lg2;
				}

				if ((v & (v - 1)) != 0)
				{
					++lg2;
				}

				return lg2;
			}
		}

		template<typename T, unsigned SizeLg2>
		struct ring_buffer
		{
			static_assert(std::is_trivially_destructible_v<T>);
			static_assert(std::is_trivially_constructible_v<T>);

			static constexpr unsigned SIZE = 1u << SizeLg2;
			static constexpr unsigned SIZE_MASK = SIZE - 1;

			std::atomic_uint32_t tail = 0;
			T buf[SIZE];
			std::atomic_uint32_t head = 0;
		};

		template<typename T>
		struct fifo_queue
		{
		private:
			static constexpr unsigned MIN_BLOCK_SIZE = 64 * 4;
			static constexpr unsigned MIN_BLOCK_COUNT_LG2 = 3;
			static constexpr unsigned BLOCK_COUNT_LG2 = std::max(lg2::RoundUp(MIN_BLOCK_SIZE / sizeof(T)), MIN_BLOCK_COUNT_LG2);
	
			static_assert(std::is_trivially_destructible_v<T>);
			static_assert(std::is_trivially_constructible_v<T>);
		public:
			using block = ring_buffer<T, BLOCK_COUNT_LG2>;
			struct node
			{
				std::atomic<node*> next;
				block value;
			};

			std::atomic<node*> head;
			uint8_t _cachePad[64 - sizeof(head)];
			std::atomic<node*> tail;
			node* first;
			node* headCopy;

			fifo_queue();
			~fifo_queue();

			fifo_queue(const fifo_queue&) = delete;
			fifo_queue(const fifo_queue&&) = delete;
			fifo_queue& operator=(const fifo_queue&) = delete;
			fifo_queue& operator=(const fifo_queue&&) = delete;
		};

		namespace ring
		{
			template<typename T, unsigned SizeLg2, typename U>
			static bool try_push(ring_buffer<T, SizeLg2>* ring, U val)
			{
				const unsigned curTail = ring->tail.load(std::memory_order_relaxed); // Only producer write to tail, so can relax this load
				const unsigned curHead = ring->head.load(std::memory_order_acquire); // Need to acquire, since consumer may change it

				if (curTail - curHead < ring->SIZE)
				{
					ring->buf[curTail & ring->SIZE_MASK] = std::move(val);
					ring->tail.store(curTail + 1, std::memory_order_release); // Release the new value to the consumer thread

					return true;
				}

				return false;
			}

			template<typename T, unsigned SizeLg2>
			static std::optional<T> try_pop(ring_buffer<T, SizeLg2>* ring)
			{
				const unsigned curTail = ring->tail.load(std::memory_order_acquire); // Acquire from the produer thread
				const unsigned curHead = ring->head.load(std::memory_order_relaxed); // Only written to by the consumer, so can relax this load

				if (curTail != curHead)
				{
					auto ret = std::optional(std::move(ring->buf[curHead & ring->SIZE_MASK]));
					ring->head.store(curHead + 1, std::memory_order_release); // Releass to the producer

					return ret;
				}

				return std::nullopt;
			}
		}

		namespace queue
		{
			namespace queue_internal
			{
				template<typename T>
				static typename fifo_queue<T>::node* alloc_node(fifo_queue<T>* q)
				{
					using node = typename fifo_queue<T>::node;
					node* ret;

					if (q->first == q->headCopy)
					{
						ret = q->first;
						q->first = q->first->next.load(std::memory_order_relaxed);
					}
					else
					{
						q->headCopy = q->head.load(std::memory_order_acquire);
						if (q->first == q->headCopy)
						{
							ret = q->first;
							q->first = q->first->next.load(std::memory_order_relaxed);
						}
						else
						{
							ret = new node;
						}
					}

					return ret;
				}
			}

			template<typename T, typename U>
			static void push(fifo_queue<T>* q, U val)
			{
				using node = typename fifo_queue<T>::node;
				node* const curTail = q->tail.load(std::memory_order_relaxed); // Only push modifies tail, so relax the oad

				sanity(curTail);

				if (!ring::try_push(&curTail->value, std::forward<U>(val)))
				{
					node* const newTail = queue_internal::alloc_node(q);
					
					newTail->value.buf[0] = std::move(val);
					newTail->value.tail.store(1, std::memory_order_release); // Release the new value to the consumer thread

					newTail->next.store(nullptr, std::memory_order_relaxed);
					curTail->next.store(newTail, std::memory_order_release);
					q->tail.store(newTail, std::memory_order_release);
				}
			}

			template<typename T>
			static std::optional<T> try_pop(fifo_queue<T>* q)
			{
				using node = typename fifo_queue<T>::node;
				node* curHead = q->head.load(std::memory_order_relaxed);

				for(;;)
				{
					std::optional<T> ret = ring::try_pop(&curHead->val);

					sanity(curHead);

					if (ret == std::nullopt)
					{
						node* const curTail = q->tail.load(std::memory_order_acquire);

						if (curTail == curHead)
						{
							sanity(curHead->next.load(std::memory_order_acquire) == nullptr);
							break;
						}
						else
						{
							node* const curHeadNext = curHead->next.load(std::memory_order_acquire);
							q->head.store(curHeadNext, std::memory_order_relaxed);
							curHead = curHeadNext;
						}
					}
					else
					{
						return ret;
					}
				}

				return std::nullopt;
			}
		}

		template<typename T>
		fifo_queue<T>::fifo_queue() : head(new node), tail(head), first(head.load(std::memory_order_relaxed)), headCopy(head.load(std::memory_order_relaxed))
		{
			headCopy->next.store(nullptr, std::memory_order_relaxed);
		}

		template<typename T>
		fifo_queue<T>::~fifo_queue()
		{
			node* n = first;

			do
			{
				node* const next = n->next.load(std::memory_order_acquire);
				delete n;
				n = next;
			} while (n);
		}
	}

	struct FreeList
	{
		FreeList* next;
	};

	struct TaskThread
	{
		std::thread thread{};

		FreeList* freeStacks = nullptr;

		// These are tasks that have been assigned to run on this
		// thread, but haven't yet started. This list should probably
		// be kept fairly small, since it runs contrarry to work
		// stealing
		spsc::ring_buffer<Task, THREAD_WAIT_QUEUE_SIZE_LG2> tasksAwaitingExecution{};

		// This is the list of new tasks created by this thread. They
		// have the potential to be run on any thread
		spsc::fifo_queue<Task> unassignedTasks{};

		// These are active tasks that were started on this thread, 
		// but which hit a wait or yield, and now are scheduled to
		// resume execution.
		spsc::fifo_queue<fiber::Fiber*> runningTasks{};

		// These are tasks which hit a wait or yield and need to be
		// rescheduled for execution on the appropriate reactor thread
		// in the case of a wait, or this thread in case of a yield
		spsc::fifo_queue<ScheduledFiber> stalledTasks{};
	};

	struct ReactorThread
	{
		std::thread thread{};

		// This is the list of tasks scheduled to run on this thread
		spsc::fifo_queue<ScheduledFiber> runningTasks{};

		// This is the list of tasks which have finished their
		// wait and now need to be returned to their parent thread
		spsc::fifo_queue<ScheduledFiber> finishedTasks{};
	};

	constexpr scheduler::Options operator&(scheduler::Options a, scheduler::Options b)
	{
		return static_cast<scheduler::Options>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
	}

	constexpr bool operator!(scheduler::Options a)
	{
		return a != scheduler::Options::NONE;
	}
}

namespace scheduler
{
	struct Scheduler
	{
		fiber::FiberAPI fiberAPI;
		TaskThread* taskThreads;
		ReactorThread* reactorThreads;
		std::atomic_uint32_t* activeTaskThreads;
		uint32_t taskThreadCount;
		uint32_t reactorThreadCount;
	};
}

namespace scheduler
{

	namespace thread
	{

	}

	namespace task
	{

	}

	Scheduler* Create(Options opts)
	{
		Scheduler* const out = new Scheduler;
		const unsigned taskThreadCount = std::thread::hardware_concurrency();

		{
			fiber::Options fiberOpts = fiber::Options::NONE;

			if (!!(opts & Options::OS_ABI_SAFE))
			{
				fiberOpts |= fiber::Options::OS_API_SAFETY;
			}
			if (!!(opts & Options::PRESERVE_FPU_CONTROL))
			{
				fiberOpts |= fiber::Options::PRESERVE_FPU_CONTROL;
			}

			out->fiberAPI = fiber::GetAPI(fiberOpts);
		}

		out->taskThreadCount = taskThreadCount;
		out->taskThreads = new TaskThread[taskThreadCount];
		out->reactorThreadCount = 0;
		out->reactorThreads = nullptr;

		const unsigned activeTaskThreadDWordCount = (taskThreadCount + 31) / 32;
		out->activeTaskThreads = new std::atomic_uint32_t[activeTaskThreadDWordCount];

		for (unsigned dwordIndex = 0; dwordIndex < activeTaskThreadDWordCount; ++dwordIndex)
		{
			out->activeTaskThreads[dwordIndex].store(~0u, std::memory_order_relaxed);
		}

		// Skip 0, that's the main thread.
		for (unsigned threadIndex = 1; threadIndex < taskThreadCount; ++threadIndex)
		{
			TaskThread* const thread = out->taskThreads + threadIndex;
			const std::wstring threadName = L"Task Thread";
			wchar_t threadIdBuf[8];

			thread->thread = std::thread(TaskThreadFunc, out, threadIndex);

			{
				std::thread::native_handle_type threadHandle = thread->thread.native_handle();

				_itow_s(threadIndex, threadIdBuf, 10);
				SetThreadAffinityMask(threadHandle, 1ull << (threadIndex & 63));
				SetThreadDescription(threadHandle, (threadName + threadIdBuf).c_str());
			}
		}
	}

	void Destroy(Scheduler* sch)
	{

	}

	void SetDefault(Scheduler* sch)
	{

	}
}