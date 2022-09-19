#pragma once

#include <cstdint>

namespace fiber
{
	struct Fiber
	{
		uintptr_t* sp;
		uintptr_t* stackCeil;
		uintptr_t* stackHead;
	};

	typedef void(*FiberFunc)(void*);

	Fiber Create(void *stack, unsigned stackSize, FiberFunc startAddress, void* userData);
	void Start(Fiber* toFiber);
	void Switch(Fiber *curFiber, Fiber* toFiber);
}