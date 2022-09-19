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

namespace
{
	static constexpr uintptr_t END_GUARD = 0xDEADDEADDEADDEADull;

	static void Validate(const fiber::Fiber& f)
	{
#if USING(VALIDATE_FIBERS)
		sanity(*f.stackCeil == END_GUARD && "Stack overrun detected.");
		sanity(f.sp >= f.stackCeil && "Stack overrun detected.");
		sanity(f.sp < f.stackHead && "Stack underrun detected.");
#else //#if USING(VALIDATE_FIBERS)
		((void)f);
#endif //#else //#if USING(VALIDATE_FIBERS)
	}
}

namespace fiber
{
	Fiber Create(void* stack, unsigned stackSize, FiberFunc startAddress, void* userData)
	{
		static constexpr unsigned CONTEXT_REG_COUNT = EXTRA_QWORD_COUNT + BASE_REG_COUNT + XMM_REG_COUNT * 2;
		uintptr_t* const stackCeil = reinterpret_cast<uintptr_t*>(stack);
		uintptr_t* const stackBase = stackCeil + (stackSize / sizeof(uintptr_t));
		Fiber out{ stackBase, stackCeil, stackBase };

		*stackCeil = END_GUARD;
		out.sp = InitStackRegisters(stackBase, startAddress, userData, stackSize);
		Validate(out);

		return out;								   
	}

	void Start(Fiber* toFiber)
	{
		sanity(toFiber->stackHead[-1] == GetStackStartPlaceholder());

		Validate(*toFiber);
		StartASM(toFiber->sp);
		Validate(*toFiber);
	}

	void Switch(Fiber* curFiber, Fiber* toFiber)
	{
		toFiber->stackHead[-1] = curFiber->stackHead[-1]; // copy around the return stack frame pointer

		Validate(*curFiber);
		Validate(*toFiber);

		SwitchFiberASM(curFiber, toFiber);

		Validate(*toFiber);
		Validate(*curFiber);
	}
}
