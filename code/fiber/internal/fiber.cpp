#if defined(_WIN32)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
#include <xmmintrin.h>
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
#include "platform_api.h"
#include "features.h"
#include "../fiber/fiber.h"

#define sanity(X) do{ if(!(X)) __debugbreak(); }while(0)

namespace fiber
{
	Fiber Create(void* stack, unsigned stackSize, FiberFunc startAddress, void* userData)
	{
		uintptr_t* const stackCeil = reinterpret_cast<uintptr_t*>(stack);
		uintptr_t* const stackBase = stackCeil + (stackSize / sizeof(uintptr_t));
		Fiber out{ stackBase, stackCeil };

		out.sp = InitStackRegisters(stackBase, startAddress, userData, stackSize);

		return out;								   
	}

	void Start(Fiber* toFiber)
	{
		sanity(toFiber->stackHead[-1] == GetStackStartPlaceholder());

		StartASM(toFiber->sp);
	}

	void Switch(Fiber* curFiber, Fiber* toFiber)
	{
		toFiber->stackHead[-1] = curFiber->stackHead[-1]; // copy around the return stack frame pointer

		SwitchFiberASM(curFiber, toFiber);
	}
}
