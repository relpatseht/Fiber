#include "platform.h"
#include "sanity.h"

#if USING(OS_WINDOWS)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#endif //#if USING(OS_WINDOWS)

#include "../scheduler/scheduler.h"
#include "../scheduler/thread.h"
#include "../scheduler/task.h"

#include "spsc_ring_buffer.h"
#include "spsc_queue.h"

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
	static constexpr unsigned THREAD_WAIT_QUEUE_SIZE_LG2 = 3;

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

	struct FreeList
	{
		FreeList* next;
	};

	struct Thread
	{
		std::thread thread{};


		unsigned id;
		std::atomic_bool hasData = false;
		uint8_t _padding[3];
	};

	struct TaskThread : public Thread
	{
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

	struct ReactorThread : public Thread
	{
		// This is the list of tasks scheduled to run on this thread
		spsc::fifo_queue<ScheduledFiber> runningTasks{};

		// This is the list of tasks which have finished their
		// wait and now need to be returned to their parent thread
		spsc::fifo_queue<ScheduledFiber> finishedTasks{};
	};
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
		std::atomic_bool running;
		uint8_t _cachePad1[64 - sizeof(running)];
		std::atomic_bool workPumpLock;
	};
}

namespace
{
	constexpr scheduler::Options operator&(scheduler::Options a, scheduler::Options b)
	{
		return static_cast<scheduler::Options>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
	}

	constexpr bool operator!(scheduler::Options a)
	{
		return a != scheduler::Options::NONE;
	}

	namespace thread
	{
		struct Context
		{
			scheduler::Scheduler* sch;
			Thread* thisThread;
			fiber::Fiber* rootFiber;
		};

		static void Wake(Thread* thread)
		{
			if (!thread->hasData.exchange(true))
			{
				WakeByAddressSingle(&thread->hasData);
			}
		}

		static void Sleep(Thread* thread)
		{
			bool trueVal = true;

			thread->hasData.store(false, std::memory_order_release);
			WaitOnAddress(&thread->hasData, &trueVal, sizeof(trueVal), INFINITE);
		}
	}

	namespace task_thread
	{
		static constexpr size_t TASK_STACK_SIZE = 4096;


		struct TaskContext
		{
			fiber::Fiber* taskFiber;
			fiber::Fiber* rootFiber;
			FreeList** freeStacks;
			const fiber::FiberAPI* fiberAPI;
			Task task;
		};

		static void FiberTask(void* userData)
		{
			TaskContext* const taskCtx = reinterpret_cast<TaskContext*>(userData);
			fiber::Fiber* const taskFiber = taskCtx->taskFiber;
			FreeList** const freeStacks = taskCtx->freeStacks;
			const fiber::FiberAPI& api = *taskCtx->fiberAPI;
			void* const taskUserData = reinterpret_cast<void*>(taskCtx->task.userDataPtr);

			taskCtx->task.TaskFunc(taskUserData);
			if (taskCtx->task.ownedPtr)
			{
				delete[] taskUserData;
			}

			uint8_t* const taskStack = reinterpret_cast<uint8_t*>(taskFiber) - (TASK_STACK_SIZE - sizeof(fiber::Fiber*));
			FreeList* const freeStack = reinterpret_cast<FreeList*>(taskStack);

			freeStack->next = *freeStacks;
			*freeStacks = freeStack;

			api.Switch(taskFiber, taskCtx->rootFiber);
		}

		namespace run
		{
			static void DrainExecuteActive(const fiber::FiberAPI& api, fiber::Fiber *rootFiber, spsc::fifo_queue<fiber::Fiber*>* activeFibers)
			{
				while (std::optional<fiber::Fiber*> nextFiber = spsc::queue::try_pop(activeFibers))
				{
					sanity(nextFiber.has_value());

					api.Switch(rootFiber, nextFiber.value());
				}
			}

			static void DrainExecuteWaiting(const fiber::FiberAPI& api, fiber::Fiber *rootFiber, FreeList **freeStacks, spsc::ring_buffer<Task, THREAD_WAIT_QUEUE_SIZE_LG2>* waitingTasks)
			{
				while (std::optional<Task> nextTask = spsc::ring::try_pop(waitingTasks))
				{
					sanity(nextTask.has_value());

					TaskContext taskCtx{ nullptr, rootFiber, freeStacks, &api, nextTask.value() };
					void* stackMem = *freeStacks;

					if (stackMem)
					{
						*freeStacks = (*freeStacks)->next;
					}
					else
					{
						stackMem = new uint8_t[TASK_STACK_SIZE];
					}

					fiber::Fiber* const newFiber = api.Create(stackMem, TASK_STACK_SIZE, &FiberTask, &taskCtx);
					taskCtx.taskFiber = newFiber;
					api.Switch(rootFiber, newFiber);
				}
			}
		}

		namespace schedule
		{
			static void DrainStalledTasks(scheduler::Scheduler* sch)
			{
				const unsigned taskThreadCount = sch->taskThreadCount;

				for (unsigned threadIndex = 0; threadIndex < taskThreadCount; ++threadIndex)
				{
					TaskThread* const thread = sch->taskThreads + threadIndex;

					while (std::optional<ScheduledFiber> fiber = spsc::queue::try_pop(&thread->stalledTasks))
					{
						sanity(fiber.has_value());
						const unsigned destIndex = fiber->threadId;
						Thread* destThread;

						if (destIndex < taskThreadCount)
						{
							sanity(destIndex == threadIndex);

							destThread = thread;
							spsc::queue::push(&thread->runningTasks, fiber->fiber);
						}
						else
						{
							const unsigned reactorIndex = destIndex - taskThreadCount;
							ReactorThread* const reactor = sch->reactorThreads + reactorIndex;

							sanity(reactorIndex < sch->reactorThreadCount);

							destThread = reactor;
							spsc::queue::push(&reactor->runningTasks, ScheduledFiber{ fiber->fiber, thread->id });
						}

						thread::Wake(destThread);
					}
				}
			}

			static void DrainReactors(scheduler::Scheduler* sch)
			{
				const unsigned taskThreadCount = sch->taskThreadCount;
				const unsigned reactorThreadCount = sch->reactorThreadCount;

				for (unsigned threadIndex = 0; threadIndex < reactorThreadCount; ++threadIndex)
				{
					ReactorThread* const thread = sch->reactorThreads + threadIndex;

					while (std::optional<ScheduledFiber> fiber = spsc::queue::try_pop(&thread->finishedTasks))
					{
						sanity(fiber.has_value());
						const unsigned destIndex = fiber->threadId;
						TaskThread* const destThread = sch->taskThreads + destIndex;

						sanity(destIndex < taskThreadCount);
						spsc::queue::push(&destThread->runningTasks, fiber->fiber);
						thread::Wake(destThread);
					}
				}
			}

			static void AssignNewTasksToThreads(scheduler::Scheduler* sch)
			{
				const unsigned taskThreadCount = sch->taskThreadCount;
				const unsigned taskThreadDWordCount = (taskThreadCount + 31) & ~31;
				TaskThread** const writeableThreads = reinterpret_cast<TaskThread**>(_alloca(sizeof(TaskThread*) * taskThreadCount));
				uint8_t* const writeableOpenSlots = reinterpret_cast<uint8_t*>(_alloca(sizeof(uint8_t) * taskThreadCount));
				unsigned writeableThreadCount = 0;

				for (unsigned activeTaskThreadDWordIndex = 0; activeTaskThreadDWordIndex < taskThreadDWordCount; ++activeTaskThreadDWordIndex)
				{
					const uint32_t activeDWordThreads = sch->activeTaskThreads[activeTaskThreadDWordIndex].load(std::memory_order_acquire);
					const unsigned threadStartIndex = activeTaskThreadDWordIndex * 32;
					const unsigned threadEndIndex = std::min(threadStartIndex + 32, taskThreadCount);

					for (unsigned threadIndex = threadStartIndex; threadIndex < threadEndIndex; ++threadIndex)
					{
						const unsigned threadBit = threadIndex & 31;

						if (activeDWordThreads & (1u << threadBit))
						{
							TaskThread* const writeThread = sch->taskThreads + threadIndex;
							const auto& writeTaskQueue = writeThread->tasksAwaitingExecution;
							const unsigned openSlots = writeTaskQueue.CAPACITY - spsc::ring::current_size(writeTaskQueue);

							sanity(openSlots < writeTaskQueue.CAPACITY);

							if ( openSlots > 0 )
							{
								writeableOpenSlots[writeableThreadCount] = static_cast<uint8_t>(openSlots);
								writeableThreads[writeableThreadCount++] = writeThread;
							}
						}
					}
				}

				sanity(writeableThreadCount <= taskThreadCount);

				// Take from each threads unassigned list and push to a write thread one by one. Not the best for
				// cache, but most fair.
				unsigned writeIndex = 0;
				for (;writeableThreadCount > 0;)
				{
					bool taskAdded = false;

					for (unsigned readThreadIndex = 0; readThreadIndex < taskThreadCount && writeableThreadCount > 0; ++readThreadIndex)
					{
						TaskThread* const readThread = sch->taskThreads + readThreadIndex;

						if (std::optional<Task> task = spsc::queue::try_pop(&readThread->unassignedTasks))
						{
							sanity(task.has_value());
							const unsigned writeThreadIndex = writeIndex % writeableThreadCount;
							TaskThread* const writeThread = writeableThreads[writeThreadIndex];
							const bool pushed = spsc::ring::try_push(&writeThread->tasksAwaitingExecution, *task);
							const uint8_t oldOpenSlots = writeableOpenSlots[writeThreadIndex]--;

							sanity(pushed);

							switch (oldOpenSlots)
							{
							case 0:
								sanity(0 && "unreachable");
							case 1:
							{
								// No more open slots on this thread, so remove it from the writeable list
								const unsigned writeableEnd = writeableThreadCount - 1;

								if (writeableEnd != writeThreadIndex)
								{
									writeableOpenSlots[writeThreadIndex] = writeableOpenSlots[writeableEnd];
									writeableThreads[writeThreadIndex] = writeableThreads[writeableEnd];
								}

								writeableThreadCount = writeableEnd;
							}
							break;
							case writeThread->tasksAwaitingExecution.CAPACITY:
								// Now has data, previously didn't. Wake up.
								thread::Wake(writeThread);
							default:
								++writeIndex;
							}

							taskAdded = true;
						}
					}

					// No new waiting tasks, end
					if (!taskAdded)
					{
						break;
					}
				}
			}
		}

		static void FiberMain(void* userData)
		{
			thread::Context* const ctx = reinterpret_cast<thread::Context*>(userData);
			fiber::FiberAPI api = ctx->sch->fiberAPI;
			TaskThread* const thisThread = reinterpret_cast<TaskThread*>(ctx->thisThread);
			FreeList** freeStacks = &thisThread->freeStacks;
			std::atomic_bool* const running = &ctx->sch->running;
			std::atomic_bool* const workPumpLock = &ctx->sch->workPumpLock;
			std::atomic_bool* const workAvailable = &thisThread->hasData;
			spsc::fifo_queue<fiber::Fiber*>* const activeFibers = &thisThread->runningTasks;
			spsc::ring_buffer<Task, THREAD_WAIT_QUEUE_SIZE_LG2>* const waitingTasks = &thisThread->tasksAwaitingExecution;

			for(;;)
			{
				run::DrainExecuteActive(api, ctx->rootFiber, activeFibers);
				run::DrainExecuteWaiting(api, ctx->rootFiber, freeStacks, waitingTasks);

				if (!workPumpLock->exchange(true, std::memory_order_acq_rel))
				{
					scheduler::Scheduler* const sch = ctx->sch;
					
					schedule::DrainStalledTasks(sch);
					schedule::DrainReactors(sch);
					schedule::AssignNewTasksToThreads(sch);

					workPumpLock->store(false, std::memory_order_release);
				}

				if (spsc::ring::current_size(*waitingTasks) == 0 && spsc::queue::is_empty(*activeFibers))
				{
					if (!running->load(std::memory_order_acquire))
					{
						break;
					}
					else
					{
						thread::Sleep(thisThread);
					}
				}
			}
		}

		static void ThreadMain(scheduler::Scheduler* sch, unsigned threadIndex)
		{
			static constexpr unsigned taskThreadStackSize = 1024; // Most likely overkill;
			uint8_t* const taskThreadStack = new uint8_t[taskThreadStackSize];
			thread::Context ctx{ sch, sch->taskThreads + threadIndex };

			sanity(threadIndex < sch->taskThreadCount);

			ctx.rootFiber = sch->fiberAPI.Create(taskThreadStack, taskThreadStackSize, FiberMain, &ctx);
			sch->fiberAPI.Start(ctx.rootFiber);
			delete[]taskThreadStack;
		}
	}

	namespace reactor_thread
	{
		static void FiberMain(void* userData)
		{
			thread::Context* const ctx = reinterpret_cast<thread::Context*>(userData);
			fiber::FiberAPI api = ctx->sch->fiberAPI;
			const unsigned taskThreadCount = ctx->sch->taskThreadCount;
			ReactorThread* const thisThread = reinterpret_cast<ReactorThread*>(ctx->thisThread);
			fiber::Fiber* const rootFiber = ctx->rootFiber;
			std::atomic_bool* const running = &ctx->sch->running;
			spsc::fifo_queue<ScheduledFiber>* const activeFibers = &thisThread->runningTasks;
			spsc::fifo_queue<ScheduledFiber>* const finishedFibers = &thisThread->finishedTasks;
			
			for (;;)
			{
				while (std::optional<ScheduledFiber> fiberThreadPair = spsc::queue::try_pop(activeFibers))
				{
					sanity(fiberThreadPair.has_value());

					sanity(fiberThreadPair->threadId < taskThreadCount);

					api.Switch(rootFiber, fiberThreadPair->fiber);
					spsc::queue::push(finishedFibers, std::move(fiberThreadPair.value()));
				}

				if (spsc::queue::is_empty(*activeFibers))
				{
					if (!running->load(std::memory_order_acquire))
					{
						break;
					}
					else
					{
						thread::Sleep(thisThread);
					}
				}
			}
		}

		static void ThreadMain(scheduler::Scheduler* sch, unsigned threadId)
		{
			static constexpr unsigned reactorThreadStackSize = 1024; // Most likely overkill;
			const unsigned threadIndex = threadId - sch->taskThreadCount;
			uint8_t* const reactorThreadStack = new uint8_t[reactorThreadStackSize];
			thread::Context ctx{ sch, sch->reactorThreads + threadIndex };

			sanity(threadId > sch->taskThreadCount);
			sanity(threadIndex < sch->reactorThreadCount);

			ctx.rootFiber = sch->fiberAPI.Create(reactorThreadStack, reactorThreadStackSize, FiberMain, &ctx);
			sch->fiberAPI.Start(ctx.rootFiber);
			delete[] reactorThreadStack;
		}
	}
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

		out->running.store(true, std::memory_order_relaxed);
		out->workPumpLock.store(true, std::memory_order_relaxed);

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

			thread->thread = std::thread(task_thread::ThreadMain, out, threadIndex);
			thread->id = threadIndex;

			{
				const std::wstring threadName = L"Task Thread ";
				std::thread::native_handle_type threadHandle = thread->thread.native_handle();
				wchar_t threadIdBuf[8];

				// Pin and name task threads
				_itow_s(threadIndex, threadIdBuf, 10);
				SetThreadIdealProcessor(threadHandle, threadIndex / 64);
				SetThreadAffinityMask(threadHandle, 1ull << (threadIndex & 63));
				SetThreadDescription(threadHandle, (threadName + threadIdBuf).c_str());
			}
		}

		return out;
	}

	void Destroy(Scheduler* sch)
	{
		sch->running.store(false, std::memory_order_release);
		WakeByAddressAll(&sch->running);

		for (unsigned threadIndex = 1; threadIndex < sch->taskThreadCount; ++threadIndex)
		{
			TaskThread* thread = sch->taskThreads + threadIndex;

			thread->thread.join();
			while (thread->freeStacks)
			{
				FreeList* const next = thread->freeStacks->next;
				delete[] thread->freeStacks;
				thread->freeStacks = next;
			}
		}

		delete[] sch->taskThreads;
		delete[] sch->reactorThreads;
		delete[] sch->activeTaskThreads;

		memset(sch, 0, sizeof(*sch));
		delete sch;
	}
}
