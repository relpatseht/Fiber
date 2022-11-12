#pragma once

#include <cstdint>

namespace fiber
{
	enum class Options : unsigned
	{
		NONE = 0,
		OS_API_SAFETY = 1<<0,
		PRESERVE_FPU_CONTROL = 1<<1
	};

	constexpr Options operator|(Options a, Options b)
	{
		return static_cast<Options>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
	}
	
	constexpr Options operator&(Options a, Options b)
	{
		return static_cast<Options>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
	}

	constexpr bool operator!(Options a)
	{
		return a != Options::NONE;
	}

	constexpr Options operator~(Options a)
	{
		return static_cast<Options>(~static_cast<unsigned>(a));
	}

	inline Options& operator|=(Options& a, Options b)
	{
		return reinterpret_cast<Options&>(reinterpret_cast<unsigned&>(a) |= static_cast<unsigned>(b));
	}
	
	inline Options& operator&=(Options& a, Options b)
	{
		return reinterpret_cast<Options&>(reinterpret_cast<unsigned&>(a) &= static_cast<unsigned>(b));
	}

	struct Fiber;

	typedef void(*FiberFunc)(void*);

	struct FiberAPI
	{
		/* Creates a new fiber with the given stack running the given function
		*  stack - pointer to the stack memory block. The end of the stack memory.
		*  stackSize - Total size of the stack memory block, in bytes
		*  commitedStackSize - Size of the currently allocated pages of the stack in bytes. 
		*                      Should be page size aligned if used. If 0, assumed the same as stackSize.
		*                      If less than stackSize, the next page must be a guard page.
		*  StartAddress - Start function for the stack
		*  userData - Data passed to StartAddress
		*/
		Fiber* (*Create)(void* stack, size_t stackSize, size_t commitedStackSize, FiberFunc StartAddress, void* userData);
		void (*Start)(Fiber* toFiber);
		void (*Switch)(Fiber* curFiber, Fiber* toFiber);
	};

	FiberAPI GetAPI(Options opts);
}