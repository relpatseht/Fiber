#pragma once

#include <cstdint>

extern void (*const StartASM)(void* fiberSP);
extern void (*const SwitchFiberASM)(void* curFiber, void* toFiber);

uintptr_t GetStackStartPlaceholder();
uintptr_t* InitStackRegisters(uintptr_t* sp, void(*StartAddress)(void*), void *userData, size_t stackSize);