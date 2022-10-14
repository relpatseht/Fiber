#pragma once

#include <atomic>
#include <optional>
#include <algorithm>
#include "sanity.h"
#include "power_two.h"
#include "spsc_ring_buffer.h"

// Adapted from Dmitry Vyukov's spsc unbounded lock-free queue

namespace spsc
{
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

		fifo_queue() : head(new node), tail(head.load(std::memory_order_relaxed)), first(head.load(std::memory_order_relaxed)), headCopy(head.load(std::memory_order_relaxed))
		{
			headCopy->next.store(nullptr, std::memory_order_relaxed);
		}

		~fifo_queue()
		{
			node* n = first;

			do
			{
				node* const next = n->next.load(std::memory_order_acquire);
				delete n;
				n = next;
			} while (n);
		}

		fifo_queue(const fifo_queue&) = delete;
		fifo_queue(const fifo_queue&&) = delete;
		fifo_queue& operator=(const fifo_queue&) = delete;
		fifo_queue& operator=(const fifo_queue&&) = delete;
	};

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

			for (;;)
			{
				std::optional<T> ret = ring::try_pop(&curHead->value);

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

		template<typename T>
		static bool is_empty(const fifo_queue<T>& q)
		{
			using node = typename fifo_queue<T>::node;
			const node* const curHead = q.head.load(std::memory_order_acquire);

			sanity(curHead);

			if (ring::current_size(curHead->value) != 0)
			{
				return false;
			}
			else
			{
				const node* const curTail = q.tail.load(std::memory_order_acquire);

				return curHead != curTail;
			}
		}
	}
}