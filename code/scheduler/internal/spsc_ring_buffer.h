#pragma once

#include <atomic>
#include <optional>

namespace spsc
{
	template<typename T, unsigned CapacityLg2>
	struct ring_buffer
	{
		static_assert(std::is_trivially_destructible_v<T>);
		static_assert(std::is_trivially_constructible_v<T>);

		static constexpr unsigned CAPACITY = 1u << CapacityLg2;
		static constexpr unsigned CAPACITY_MASK = CAPACITY - 1;

		std::atomic_uint32_t tail = 0;
		T buf[CAPACITY];
		std::atomic_uint32_t head = 0;
	};

	namespace ring
	{
		template<typename T, unsigned CapacityLg2, typename U>
		static bool try_push(ring_buffer<T, CapacityLg2>* ring, U val)
		{
			const unsigned curTail = ring->tail.load(std::memory_order_relaxed); // Only producer write to tail, so can relax this load
			const unsigned curHead = ring->head.load(std::memory_order_acquire); // Need to acquire, since consumer may change it

			if (curTail - curHead < ring->CAPACITY)
			{
				ring->buf[curTail & ring->SIZE_MASK] = std::move(val);
				ring->tail.store(curTail + 1, std::memory_order_release); // Release the new value to the consumer thread

				return true;
			}

			return false;
		}

		template<typename T, unsigned CapacityLg2>
		static std::optional<T> try_pop(ring_buffer<T, CapacityLg2>* ring)
		{
			const unsigned curTail = ring->tail.load(std::memory_order_acquire); // Acquire from the produer thread
			const unsigned curHead = ring->head.load(std::memory_order_relaxed); // Only written to by the consumer, so can relax this load

			if (curTail != curHead)
			{
				auto ret = std::optional(std::move(ring->buf[curHead & ring->CAPACITY_MASK]));
				ring->head.store(curHead + 1, std::memory_order_release); // Releass to the producer

				return ret;
			}

			return std::nullopt;
		}

		template<typename T, unsigned CapacityLg2>
		static unsigned current_size(const ring_buffer<T, CapacityLg2>& ring)
		{
			const unsigned curTail = ring.tail.load(std::memory_order_acquire); 
			const unsigned curHead = ring.head.load(std::memory_order_acquire);

			return ring.CAPACITY - (curTail - curHead);
		}

		template<typename T, unsigned CapacityLg2>
		static constexpr unsigned capacity(const ring_buffer<T, CapacityLg2>& r)
		{
			return r.CAPACITY;
		}
	}
}