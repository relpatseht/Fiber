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
	uintptr_t GetShutdownAddrASM();
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

		*--out.sp = GetShutdownAddrASM();
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

out.sp[28] = 0x5151515151515151ull; // rsi
out.sp[27] = 0xD1D1D1D1D1D1D1D1ull; // rdi
out.sp[26] = 0xB9B9B9B9B9B9B9B9ull; // rbp
out.sp[25] = 0xBBBBBBBBBBBBBBBBull; // rbx
out.sp[24] = 0x1212121212121212ull; // r12
out.sp[23] = 0x1313131313131313ull; // r13
out.sp[22] = 0x1414141414141414ull; // r14
out.sp[21] = 0x1515151515151515ull; // r15
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x98, 0x06, 16); // xmm6
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x88, 0x07, 16); // xmm7
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x78, 0x08, 16); // xmm8
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x68, 0x09, 16); // xmm9
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x58, 0x10, 16); // xmm10
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x48, 0x11, 16); // xmm11
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x38, 0x12, 16); // xmm12
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x28, 0x13, 16); // xmm13
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x18, 0x14, 16); // xmm14
std::memset(reinterpret_cast<uint8_t*>(out.sp) + 0x08, 0x15, 16); // xmm15
out.sp[0] = 0xBEADEADBEE0BEEF0ull; // padding
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

#if 0
namespace fiber
{
#if defined(_WIN32)
	struct Fiber {}; 
#elif defined(__linux__) //#if defined(_WIN32)
	struct Fiber
	{
		ucontext_t fiber;
		jmp_buf jmp;
	};
#endif //#elif defined(__linux__) //#if defined(_WIN32)
}

namespace
{
	using namespace fiber;

#if defined(_WIN32)
#elif defined(__linux__) //#if defined(_WIN32)
	struct FiberCtx
	{
		FiberFunc callback;
		void* userData;
		jmp_buf* jmp;
		ucontext_t* prevCtx;
	};

	static void FiberCallbackKickoff(void* voidCtx)
	{
		FiberCtx* const ctx = reinterpret_cast<FiberCtx*>(voidCtx);
		const FiberFunc callback = ctx->callback;
		void* const userData = ctx->userData;

		if (_setjmp(*ctx->jmp) == 0)
		{
			ucontext_t tmp;
			swapcontext(&tmp, ctx->prevCtx);
		}

		callback(userData);
	}

	namespace mem
	{
		static constexpr size_t PAGE_SIZE = 4096;
		static constexpr size_t PAGE_SIZE_MASK = PAGE_SIZE - 1;

		static void* MallocOverwriteProtection(size_t size)
		{
			static constexpr size_t MEM_FLAGS = MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK | MAP_UNINITIALIZED;
			const size_t overheadSize = size + sizeof(size_t); // Need to store alloc length
			const size_t pageAlignedSize = (overheadSize + PAGE_SIZE_MASK) & ~PAGE_SIZE_MASK;
			const size_t requestSize = pageAlignedSize + PAGE_SIZE; // second page to protect
			void* const mem = mmap(nullptr, requestSize, PROT_NONE, MEM_FLAGS, -1, 0); // Allocate protected memory
			uint8_t* const u8Mem = reinterpret_cast<uint8_t*>(mem);
			uint8_t* const retAddr = u8Mem + (pageAlignedSize - size); // The end of the allocation needs to be against the protected memory
			size_t* const sizeAddr = reinterpret_cast<size_t*>(retAddr - sizeof(size_t));

			sanity((reinterpret_cast<uintptr_t>(retAddr + size)& PAGE_SIZE_MASK) == 0);
			sanity(pageAlignedSize - size < PAGE_SIZE);

			*sizeAddr = requestSize;
			mprotect(mem, pageAlignedSize, PROT_READ | PROT_WRITE); // Mark the useable portion read/write

			return retAddr;
		}

		static void FreeOverwriteProtectedMem(void* mem)
		{
			size_t* const sizeTMem = reinterpret_cast<size_t*>(mem);
			const uintptr_t addr = reinterpret_cast<uintptr_t>(mem);
			const uintptr_t pageAddr = addr & PAGE_SIZE_MASK;
			void* const freeAddr = reinterpret_cast<void*>(addr);
			const size_t freeSize = sizeTMem[-1];

			munmap(freeAddr, freeSize);
		}
	}
#endif //#elif defined(__linux__) //#if defined(_WIN32)
}

namespace fiber
{
#if defined(_WIN32)
	Fiber* InitForThread()
	{
		reinterpret_cast<Fiber*>(ConvertThreadToFiber(nullptr));
	}

	Fiber* Create(unsigned stackSize, FiberFunc startAddress, void* userData)
	{
		reinterpret_cast<Fiber*>(CreateFiber(stackSize, startAddress, userData));
	}

	void Destroy(Fiber* fiber)
	{
		DeleteFiber(fiber);
	}

	void SwitchToFiber(const Fiber* /*curFiber*/, Fiber* toFiber)
	{
		::SwitchToFiber(toFiber);
	}
#elif defined(__linux__) //#if defined(_WIN32)
	Fiber* InitForThread()
	{
		static thread_local Fiber threadFiber;

		return threadFiber;
	}

	Fiber* Create(unsigned stackSize, FiberFunc startAddress, void* userData)
	{
		static void(*StartFunc)() = reinterpret_cast<void(*)()>(FiberCallbackKickoff);
		const size_t totalAlloc = stackSize + sizeof(Fiber);
		void* const mem = mem::MallocOverwriteProtection(totalAlloc);
		Fiber* const fiber = reinterpret_cast<Fiber*>(mem);

		fiber->fiber.uc_stack.ss_sp = reinterpret_cast<uint8_t>(mem) + sizeof(fiber);
		fiber->fiber.uc_stack.ss_size = stackSize;
		fiber->fiber.uc_link = 0;

		ucontext_t tmp;
		FiberCtx ctx;
		ctx.callback = startAddress;
		ctx.userData = userData;
		ctx.jmp = &fiber->jmp;
		ctx.prevCtx = &tmp;
		makecontext(&fiber->fiber, StartFunc, 1, &ctx);
		swapcontext(&tmp, &fiber->fiber);
	}

	void Destroy(Fiber* fiber)
	{
		mem::FreeOverwriteProtectedMem(fiber);
	}

	void SwitchToFiber(const Fiber* curFiber, Fiber* toFiber)
	{
		if (_setjmp(curFiber->jmp) == 0)
			_longjmp(toFiber->jmp, 1);
	}
#else //#elif defined(__linux__) //#if defined(_WIN32)
# error Unknown OS
#endif //#else //#elif defined(__linux__) //#if defined(_WIN32)
}
#endif