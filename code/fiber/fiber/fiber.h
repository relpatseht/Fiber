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
		Fiber* (*Create)(void* stack, size_t stackSize, FiberFunc StartAddress, void* userData);
		void (*Start)(Fiber* toFiber);
		void (*Switch)(Fiber* curFiber, Fiber* toFiber);
	};

	FiberAPI GetAPI(Options opts);
}