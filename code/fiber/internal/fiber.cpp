#include <cstring>
#include "platform.h"
#include "register_definitions.h"
#include "concat_arrays.h"
#include "../fiber/fiber.h"
#if USING(OS_WINDOWS)
# define WIN32_LEAN_AND_MEAN
# define NOMINMAX
# include <Windows.h>
# include <xmmintrin.h>
#elif USING(OS_LINUX) //#if USING(OS_WINDOWS)
# include <sys/mman.h>
# include <setjmp.h>
# include <signal.h>
# include <ucontext.h>
# undef _FORTIFY_SOURCE
#endif //#elif USING(OS_LINUX) //#if USING(OS_WINDOWS)

#define sanity(X) do{ if(!(X)) __debugbreak(); }while(0)

#if USING(PROC_X64)
# define BYTECODE_INL "x64_fiber_bytecode.inl"
#elif USING(PROC_X86) //#if USING(PROC_X64)
# define BYTECODE_INL "x86_fiber_bytecode.inl"
#elif USING(PROC_ARM64) //#elif USING(PROC_X86) //#if USING(PROC_X64)
# define BYTECODE_INL "arm64_fiber_bytecode.inl"
#elif USING(PROC_ARM) //#elif USING(PROC_ARM64) //#elif USING(PROC_X86) //#if USING(PROC_X64)
# define BYTECODE_INL "arm_fiber_bytecode.inl"
#else //#elif USING(PROC_ARM) //#elif USING(PROC_ARM64) //#elif USING(PROC_X86) //#if USING(PROC_X64)
# error Unknown processor.
#endif //#else //#elif USING(PROC_ARM) //#elif USING(PROC_ARM64) //#elif USING(PROC_X86) //#if USING(PROC_X64)

namespace fiber
{
	struct Fiber
	{
		uintptr_t* sp;
	};
}

namespace
{
	using StartASMProc = void(void*);
	using SwitchASMProc = void(void*, void*);

	static uintptr_t GetStackStartPlaceholder()
	{
		return 0xBAADF00DDEADBEEFull;
	}

	static constexpr uint8_t B1(uint32_t val) { return val & 0xFF; }
	static constexpr uint8_t B2(uint32_t val) { return (val >> 8) & 0xFF; }
	static constexpr uint8_t B3(uint32_t val) { return (val >> 16) & 0xFF; }
	static constexpr uint8_t B4(uint32_t val) { return (val >> 24) & 0xFF; }

#define TO_BYTES(X) B1((X)), B2((X)), B3((X)), B4((X))

	template<fiber::Options Opts>
	struct FiberASMAPI;

	template<>
	struct FiberASMAPI<fiber::Options::NONE>
	{
#undef OPT_OS_API_SAFETY
#undef OPT_PRESERVE_FPU_CONTROL
#include BYTECODE_INL
	};

	template<>
	struct FiberASMAPI<fiber::Options::OS_API_SAFETY>
	{
#define OPT_OS_API_SAFETY 1
#include BYTECODE_INL
#undef OPT_OS_API_SAFETY
	};

	template<>
	struct FiberASMAPI<fiber::Options::PRESERVE_FPU_CONTROL>
	{
#define OPT_PRESERVE_FPU_CONTROL 1
#include BYTECODE_INL
#undef OPT_PRESERVE_FPU_CONTROL
	};

	template<>
	struct FiberASMAPI<fiber::Options::OS_API_SAFETY | fiber::Options::PRESERVE_FPU_CONTROL>
	{
#define OPT_OS_API_SAFETY 1
#define OPT_PRESERVE_FPU_CONTROL 1
#include BYTECODE_INL
#undef OPT_OS_API_SAFETY
#undef OPT_PRESERVE_FPU_CONTROL
	};

#undef TO_BYTES

	static uintptr_t* ToStackHead(fiber::Fiber* fiber)
	{
		static constexpr size_t ALIGN_STACK_ENTRIES = STACK_ALIGN / sizeof(uintptr_t);
		static constexpr size_t FIBER_STACK_ENTRIES = (sizeof(fiber::Fiber) + (sizeof(uintptr_t)-1)) / sizeof(uintptr_t);

		static_assert((STACK_ALIGN % sizeof(uintptr_t)) == 0);

		return reinterpret_cast<uintptr_t*>(fiber) - (ALIGN_STACK_ENTRIES - FIBER_STACK_ENTRIES);
	}

	template<fiber::Options Opts>
	struct FiberAPIImpl
	{
		static fiber::Fiber* Create(void* stack, size_t stackSize, fiber::FiberFunc startAddress, void* userData)
		{
			static constexpr unsigned STACK_ALIGN_MASK = STACK_ALIGN - 1;
			const uintptr_t stackAddr = reinterpret_cast<uintptr_t>(stack);
			const uintptr_t alignedStackAddr = (stackAddr + STACK_ALIGN_MASK) & ~STACK_ALIGN_MASK;
			const size_t alignedStackMemSize = stackSize - (alignedStackAddr - stackAddr);
			uintptr_t* const stackCeil = reinterpret_cast<uintptr_t*>(alignedStackAddr);
			const size_t stackMemEntries = alignedStackMemSize / sizeof(uintptr_t);
			const size_t stackEntries = stackMemEntries - (STACK_ALIGN/sizeof(uintptr_t));
			uintptr_t* const stackBase = stackCeil + stackEntries;
			fiber::Fiber* out = reinterpret_cast<fiber::Fiber*>(stackCeil + stackMemEntries - sizeof(fiber::Fiber)/sizeof(uintptr_t));

			static_assert(sizeof(fiber::Fiber) <= STACK_ALIGN);
			sanity(stackBase == ToStackHead(out));

			out->sp = FiberASMAPI<Opts>::InitStackRegisters(stackBase, startAddress, userData, stackEntries*sizeof(uintptr_t));

			return out;
		}

		static void Start(fiber::Fiber* toFiber)
		{
			const uintptr_t* const stackHead = ToStackHead(toFiber);

			sanity(stackHead[-1] == GetStackStartPlaceholder());

			FiberASMAPI<Opts>::StartASM(toFiber->sp);
		}

		static void Switch(fiber::Fiber* curFiber, fiber::Fiber* toFiber)
		{
			uintptr_t* const toStackHead = ToStackHead(toFiber);
			const uintptr_t* const curStackHead = ToStackHead(curFiber);

			toStackHead[-1] = curStackHead[-1]; // copy around the return stack frame pointer

			FiberASMAPI<Opts>::SwitchFiberASM(curFiber, toFiber);
		}

		static fiber::FiberAPI GetAPI()
		{
			fiber::FiberAPI api{};
			api.Create = &Create;
			api.Start = &Start;
			api.Switch = &Switch;
			return api;
		}
	};
}

namespace fiber
{
	FiberAPI GetAPI(Options opts)
	{
		switch (opts)
		{
			case Options::NONE: return FiberAPIImpl<Options::NONE>::GetAPI();
			case Options::OS_API_SAFETY: return FiberAPIImpl<Options::OS_API_SAFETY>::GetAPI();
			case Options::PRESERVE_FPU_CONTROL: return FiberAPIImpl<Options::PRESERVE_FPU_CONTROL>::GetAPI();
			case Options::OS_API_SAFETY | Options::PRESERVE_FPU_CONTROL: return FiberAPIImpl<Options::OS_API_SAFETY | Options::PRESERVE_FPU_CONTROL>::GetAPI();
		}

		sanity(0 && "Unknown options");

		return FiberAPI{};
	}
}
