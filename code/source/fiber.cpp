#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#elif defined(__linux__) //#if defined(_WIN32)
# include <sys/mman.h>
# include <setjmp.h>
# include <signal.h>
# include <ucontext.h>
# undef _FORTIFY_SOURCE
#else //#elif defined(__linux__) //#if defined(_WIN32)
# error Unknown OS
#endif //#else //#elif defined(__linux__) //#if defined(_WIN32)
#include <cstring>
#include "platform.h"
#include "Fiber/fiber.h"

#define sanity(X) do{ if(!(X)) __debugbreak(); }while(0)

extern "C"
{
	void StartASM(void* fiberSP);
	void SwitchFiberASM(void* curFiber, void* toFiber);
	uintptr_t GetStartupAddrASM();
}

namespace
{
	static constexpr uintptr_t START_STACK_PLACEHOLDER = 0xBAADF00D;
}

namespace fiber
{
	Fiber Create(void* stack, unsigned stackSize, FiberFunc startAddress, void* userData)
	{
		uintptr_t* const stackCeil = reinterpret_cast<uintptr_t*>(stack);
		uintptr_t* const stackBase = stackCeil + (stackSize / sizeof(uintptr_t));
		Fiber out{ stackBase, stackBase };

		*--out.sp = START_STACK_PLACEHOLDER; // placeholder for original stack set in Start

#if defined(_WIN32)
		out.sp -= 4; // argument shadow space + alignment
#endif //#if defined(_WIN32)

		* --out.sp; // Holding for the return function address
		*--out.sp = reinterpret_cast<uintptr_t>(startAddress);
		*--out.sp = reinterpret_cast<uintptr_t>(userData);
		*--out.sp = GetStartupAddrASM();
		unsigned registersToInit = CONTEXT_REG_COUNT;
#if defined(_WIN32)
		// Win32 has the TIB to init
		*--out.sp = 0; // gs::0x0, structured exception handling frame
		*--out.sp = reinterpret_cast<uintptr_t>(stackBase); // gs:0x8, stack base (high address)
		*--out.sp = reinterpret_cast<uintptr_t>(stackCeil); // gs:0x10, stack ceiling (low address)
		registersToInit -= 3;
#endif //#if defined(_WIN32)
		out.sp -= registersToInit;
		std::memset(out.sp, 0, registersToInit * sizeof(*out.sp));

		return out;								   
	}

	void Start(Fiber* toFiber)
	{
		sanity(toFiber->stackHead[-1] == START_STACK_PLACEHOLDER);

		StartASM(toFiber->sp);
	}

	void Switch(Fiber* curFiber, Fiber* toFiber)
	{
		toFiber->stackHead[-1] = curFiber->stackHead[-1]; // copy around the return stack frame pointer

		SwitchFiberASM(curFiber, toFiber);
	}
}
