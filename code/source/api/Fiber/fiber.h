#pragma once

namespace fiber
{
	struct Fiber
	{
		void** sp;
	};

	typedef void(*FiberFunc)(void*);

	Fiber Create(void *stack, unsigned stackSize, FiberFunc startAddress, void* userData);
	void Switch(const Fiber &curFiber, Fiber* toFiber);
}