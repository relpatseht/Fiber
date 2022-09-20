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

	template<fiber::Options Opts>
	struct FiberAPIImpl
	{
		static fiber::Fiber Create(void* stack, size_t stackSize, fiber::FiberFunc startAddress, void* userData)
		{
			uintptr_t* const stackCeil = reinterpret_cast<uintptr_t*>(stack);
			uintptr_t* const stackBase = stackCeil + (stackSize / sizeof(uintptr_t));
			fiber::Fiber out{ stackBase, stackCeil };

			out.sp = FiberASMAPI<Opts>::InitStackRegisters(stackBase, startAddress, userData, stackSize);

			return out;
		}

		static void Start(fiber::Fiber* toFiber)
		{
			sanity(toFiber->stackHead[-1] == GetStackStartPlaceholder());

			FiberASMAPI<Opts>::StartASM(toFiber->sp);
		}

		static void Switch(fiber::Fiber* curFiber, fiber::Fiber* toFiber)
		{
			toFiber->stackHead[-1] = curFiber->stackHead[-1]; // copy around the return stack frame pointer

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
