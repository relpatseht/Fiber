#include "platform.h"
#include "platform_api.h"
#include "register_definitions.h"
#include "features.h"
#include "concat_arrays.h"
#include <cstdint>

#if USING(SAVE_FPU_CONTROL)
#include <xmmintrin.h>
#endif //#if USING(SAVE_FPU_CONTROL)

static constexpr uint32_t TIB_ENTRIES = TIB_SEH_ENTRIES + TIB_STACK_ENTRIES;
static constexpr uint32_t PUSH_ENTRIES = CPU_REG_COUNT + TIB_ENTRIES;
static constexpr uint32_t PUSH_SIZE = PUSH_ENTRIES * CPU_REG_WIDTH;

static constexpr bool MOV_MISALIGNMENT = (PUSH_SIZE & (FPU_REG_WIDTH - 1)) == 0; // Stack grows down, so we need the fpu reg alignment to not divide equally into the current stack offset
static constexpr uint32_t MOV_FPU_CONTROL_SIZE = FPU_CONTROL_ENTRIES * 4;
static constexpr uint32_t MOV_FPU_SIZE_RAW = FPU_REG_COUNT * FPU_REG_WIDTH;
static constexpr uint32_t MOV_SIZE_RAW = (MOV_MISALIGNMENT ? std::max(MOV_FPU_CONTROL_SIZE, FPU_REG_WIDTH/CPU_REG_WIDTH) : MOV_FPU_CONTROL_SIZE) + MOV_FPU_SIZE_RAW;
static constexpr uint32_t MOV_SIZE_ALIGNED = (MOV_SIZE_RAW + (STACK_ALIGN - 1)) & ~(STACK_ALIGN - 1);
static constexpr uint32_t FPU_CONTROL_POS = MOV_SIZE_ALIGNED - CPU_REG_WIDTH;

#if USING(OS_WINDOWS)
static constexpr uint32_t STACK_RESERVED_ENTRIES = 5; // 1 for alignment from the start placeholder, 4 for shadow space
#elif USING(OS_LINUX)
static constexpr uint32_t STACK_RESERVED_ENTRIES = 1; // 1 for alignment from the start placeholder
#endif
static constexpr uint32_t STACK_RESERVE_SIZE = STACK_RESERVED_ENTRIES * CPU_REG_WIDTH;

static constexpr uint32_t INIT_FUNC_CALL_ENTRIES = 3; // 3 = return address + startAddress + userData
static constexpr uint32_t INIT_FUNC_CALL_SIZE = INIT_FUNC_CALL_ENTRIES * CPU_REG_WIDTH;
static constexpr uint32_t INIT_STACK_SIZE = PUSH_SIZE + MOV_SIZE_ALIGNED + STACK_RESERVE_SIZE + INIT_FUNC_CALL_SIZE;

static constexpr uint8_t B1(uint32_t val) { return val & 0xFF; }
static constexpr uint8_t B2(uint32_t val) { return (val >> 8) & 0xFF; }
static constexpr uint8_t B3(uint32_t val) { return (val >> 16) & 0xFF; }
static constexpr uint8_t B4(uint32_t val) { return (val >> 24) & 0xFF; }

#define TO_BYTES(X) B1((X)), B2((X)), B3((X)), B4((X))

// ToDo: I'm being lazy on linux. I could use a byte width add and a byte width rsp
//       offset if saving the FPU control.

static constexpr const uint8_t StoreContextASM[] = {
	0x58,                                              //pop rax; Pop off our return address
#if USING(SAVE_TIB_SEH)								   
	0x65, 0xFF, 0x34, 0x25, 0x00, 0x00, 0x00, 0x00,    //push QWORD PTR gs:[00h]
#endif //#if USING(SAVE_TIB_SEH)					   
#if USING(SAVE_TIB_STACK)							   
	0x65, 0xFF, 0x34, 0x25, 0x08, 0x00, 0x00, 0x00,    //push QWORD PTR gs:[08h]
	0x65, 0xFF, 0x34, 0x25, 0x10, 0x00, 0x00, 0x00,    //push QWORD PTR gs:[10h]
	0x65, 0xFF, 0x34, 0x25, 0x78, 0x14, 0x00, 0x00,    //push QWORD PTR gs:[1478h]
	0x65, 0xFF, 0x34, 0x25, 0x48, 0x17, 0x00, 0x00,    //push QWORD PTR gs:[1748h]
#endif //#if USING(SAVE_TIB_STACK)					   
#if USING(OS_WINDOWS)								   
	0x56,                                              //push rsi
	0x57,                                              //push rdi
#endif //#if USING(OS_WINDOWS)						   
	0x55,                                              //push rbp
	0x53,                                              //push rbx
	0x41, 0x54,                                        //push r12
	0x41, 0x55,                                        //push r13
	0x41, 0x56,                                        //push r14
	0x41, 0x57,                                        //push r15
#if USING(OS_WINDOWS) || USING(SAVE_FPU_CONTROL)
	0x48, 0x81, 0xEC, TO_BYTES(MOV_SIZE_ALIGNED),      //sub rsp, xmmRegSize; for xmm6 - xmm15
#endif //#if USING(OS_WINDOWS) || USING(SAVE_FPU_CONTROL)
#if USING(SAVE_FPU_CONTROL)
	0x0F, 0xAE, 0x9C, 0x24, TO_BYTES(FPU_CONTROL_POS), //stmxcsr[rsp + A8h]
#endif //#if USING(SAVE_FPU_CONTROL)
#if USING(OS_WINDOWS)
	0x0F, 0x29, 0xB4, 0x24, 0x98, 0x00, 0x00, 0x00,    //movaps[rsp + 98h], xmm6
	0x0F, 0x29, 0xBC, 0x24, 0x88, 0x00, 0x00, 0x00,    //movaps[rsp + 88h], xmm7
	0x44, 0x0F, 0x29, 0x44, 0x24, 0x78,                //movaps[rsp + 78h], xmm8
	0x44, 0x0F, 0x29, 0x4C, 0x24, 0x68,                //movaps[rsp + 68h], xmm9
	0x44, 0x0F, 0x29, 0x54, 0x24, 0x58,                //movaps[rsp + 58h], xmm10
	0x44, 0x0F, 0x29, 0x5C, 0x24, 0x48,                //movaps[rsp + 48h], xmm11
	0x44, 0x0F, 0x29, 0x64, 0x24, 0x38,                //movaps[rsp + 38h], xmm12
	0x44, 0x0F, 0x29, 0x6C, 0x24, 0x28,                //movaps[rsp + 28h], xmm13
	0x44, 0x0F, 0x29, 0x74, 0x24, 0x18,                //movaps[rsp + 18h], xmm14
	0x44, 0x0F, 0x29, 0x7C, 0x24, 0x08,                //movaps[rsp + 08h], xmm15
#endif //#if USING(OS_WINDOWS)					       
	0xFF, 0xE0,                                        //jmp rax; return
};

static constexpr const uint8_t InitFiberASM[] = {
	0x59,       //pop rcx; Startup userdata
	0x58,       //pop rax; Startup function
	0xFF, 0xD0, //call rax; Call the startup function.When it returns, it will hit EndFiber
};

static_assert(STACK_RESERVE_SIZE <= 0x7F, "Can't use a byte add. Need a word or dword add.");
static constexpr const uint8_t EndFiberASM[] = {
	0x48, 0x83, 0xC4, B1(STACK_RESERVE_SIZE), //add rsp, stackReserve; Remove the shadow space
	0x5C,                                     //pop rsp
};

static constexpr const uint8_t LoadContextASM[] = {
#if USING(OS_WINDOWS)
	0x44, 0x0F, 0x28, 0x7C, 0x24, 0x08,                //movaps xmm15,[rsp + 08h]
	0x44, 0x0F, 0x28, 0x74, 0x24, 0x18,                //movaps xmm14,[rsp + 18h]
	0x44, 0x0F, 0x28, 0x6C, 0x24, 0x28,                //movaps xmm13,[rsp + 28h]
	0x44, 0x0F, 0x28, 0x64, 0x24, 0x38,                //movaps xmm12,[rsp + 38h]
	0x44, 0x0F, 0x28, 0x5C, 0x24, 0x48,                //movaps xmm11,[rsp + 48h]
	0x44, 0x0F, 0x28, 0x54, 0x24, 0x58,                //movaps xmm10,[rsp + 58h]
	0x44, 0x0F, 0x28, 0x4C, 0x24, 0x68,                //movaps xmm9,[rsp + 68h]
	0x44, 0x0F, 0x28, 0x44, 0x24, 0x78,                //movaps xmm8,[rsp + 78h]
	0x0F, 0x28, 0xBC, 0x24, 0x88, 0x00, 0x00, 0x00,    //movaps xmm7,[rsp + 88h]
	0x0F, 0x28, 0xB4, 0x24, 0x98, 0x00, 0x00, 0x00,    //movaps xmm6,[rsp + 98h]
#endif //#if USING(OS_WINDOWS)
#if USING(SAVE_FPU_CONTROL)
	0x0F, 0xAE, 0x94, 0x24, TO_BYTES(FPU_CONTROL_POS), //ldmxcsr [rsp + A8h]
#endif //# if USING(SAVE_FPU_CONTROL)
#if USING(OS_WINDOWS) || USING(SAVE_FPU_CONTROL)
	0x48, 0x81, 0xC4, TO_BYTES(MOV_SIZE_ALIGNED),      //add rsp, stackRsrve
#endif //#if USING(OS_WINDOWS) || USING(SAVE_FPU_CONTROL)
	0x41, 0x5F,                                        //pop r15
	0x41, 0x5E,                                        //pop r14
	0x41, 0x5D,                                        //pop r13
	0x41, 0x5C,                                        //pop r12
	0x5B,                                              //pop rbx
	0x5D,                                              //pop rbp
#if USING(OS_WINDOWS)								   
	0x5F,                                              //pop rdi
	0x5E,                                              //pop rsi
#endif //#if USING(OS_WINDOWS)
#if USING(SAVE_TIB_STACK)
	0x65, 0x8F, 0x04, 0x25, 0x48, 0x17, 0x00, 0x00,    //pop QWORD PTR gs:[1748h]
	0x65, 0x8F, 0x04, 0x25, 0x78, 0x14, 0x00, 0x00,    //pop QWORD PTR gs:[1478h]
	0x65, 0x8F, 0x04, 0x25, 0x10, 0x00, 0x00, 0x00,    //pop QWORD PTR gs:[10h]
	0x65, 0x8F, 0x04, 0x25, 0x08, 0x00, 0x00, 0x00,    //pop QWORD PTR gs:[08h]
#endif //#if USING(SAVE_TIB_STACK)					   
#if USING(SAVE_TIB_SEH)								   
	0x65, 0x8F, 0x04, 0x25, 0x00, 0x00, 0x00, 0x00,    //pop QWORD PTR gs:[00h]
#endif //#if USING(SAVE_TIB_SEH)					   
	0xC3,                                              //ret
};

// Order StoreContextASM, StartFiberASM, SwitchToFiberASM, InitFiberASM, EndFiberASM, LoadContextASM
static constexpr uint32_t SwitchToFiberASM_Size = 13;
static constexpr uint32_t StoreContextASM_Offset = 0;
static constexpr uint32_t StartFiberASM_Offset = StoreContextASM_Offset + sizeof(StoreContextASM);

static constexpr uint32_t StartFiberASM_CallEnd_Offset = StartFiberASM_Offset + 5;
static constexpr uint32_t StartFiber_Call_StoreContext_Offset = static_cast<unsigned>(-static_cast<int>(StartFiberASM_CallEnd_Offset - StoreContextASM_Offset));
static constexpr uint32_t StartFiberASM_Jmp_LoadContext_Offset = SwitchToFiberASM_Size + sizeof(InitFiberASM) + sizeof(EndFiberASM);
static_assert(StartFiberASM_Jmp_LoadContext_Offset <= 0x7F, "Jump won't fit in a byte jump (EB), needs to use a dword jump (E9) instead");
static constexpr const uint8_t StartFiberASM[] = {
	0xE8, TO_BYTES(StartFiber_Call_StoreContext_Offset), //call StoreContext
	0x48, 0x89, 0xA1, TO_BYTES(INIT_STACK_SIZE),         //mov[rcx + totalInitStackSize], rsp; Put current stack in initial fiber state
	0x48, 0x89, 0xCC,                                    //mov rsp, rcx; Switch out to new stackframe
	0xEB, B1(StartFiberASM_Jmp_LoadContext_Offset),      //jmp LoadContext
};

static constexpr uint32_t SwitchToFiberASM_Offset = StartFiberASM_Offset + sizeof(StartFiberASM);
static constexpr uint32_t SwitchToFiberASM_CallEnd_Offset = SwitchToFiberASM_Offset + 5;
static constexpr uint32_t SwitchToFiber_Call_StoreConetxt_Offset = static_cast<unsigned>(-static_cast<int>(SwitchToFiberASM_CallEnd_Offset - StoreContextASM_Offset));
static constexpr uint32_t SwitchToFiberASM_Jmp_LoadContext_Offset = sizeof(InitFiberASM) + sizeof(EndFiberASM);
static_assert(SwitchToFiberASM_Jmp_LoadContext_Offset <= 0x7F, "Won't fit in byte jump (EB), needs dword jump (E9)");
static constexpr const uint8_t SwitchToFiberASM[] = {
	0xE8, TO_BYTES(SwitchToFiber_Call_StoreConetxt_Offset), //call StoreContext
	0x48, 0x89, 0x21,                                       //mov[rcx], rsp; Store the current stackframe
	0x48, 0x8B, 0x22,                                       //mov rsp,[rdx]; Switch to the new stackframe
	0xEB, B1(SwitchToFiberASM_Jmp_LoadContext_Offset),      //jmp LoadContext
};
static_assert(sizeof(SwitchToFiberASM) == SwitchToFiberASM_Size);

static constexpr uint32_t InitFiberASM_Offset = SwitchToFiberASM_Offset + sizeof(SwitchToFiberASM);

CODE_SEG_START
inline constexpr CODE_SEG_ATTR auto ASMBlob = concat_arrays(StoreContextASM, StartFiberASM, SwitchToFiberASM, InitFiberASM, EndFiberASM, LoadContextASM); // This concat order is important and cannot change.

using StartASMProc = void(void*);
using SwitchASMProc = void(void*, void*);
StartASMProc * const StartASM = reinterpret_cast<StartASMProc*>(ASMBlob.data() + StartFiberASM_Offset);
SwitchASMProc* const SwitchFiberASM = reinterpret_cast<SwitchASMProc*>(ASMBlob.data() + SwitchToFiberASM_Offset);

uintptr_t GetStackStartPlaceholder()
{
	return 0xBAADF00DDEADBEEFull;
}

uintptr_t* InitStackRegisters(uintptr_t* sp, void(*StartAddress)(void*), void* userData, size_t stackSize)
{
	uintptr_t* const spBase = sp;

	*--sp = GetStackStartPlaceholder(); // Placeholder for original stack pointer, which will be set in InitFiberASM

	sp -= STACK_RESERVED_ENTRIES; // Shadowspace and return address will go here

	*--sp = reinterpret_cast<uintptr_t>(StartAddress);
	*--sp = reinterpret_cast<uintptr_t>(userData);
	*--sp = reinterpret_cast<uintptr_t>(ASMBlob.data() + InitFiberASM_Offset);

#if USING(SAVE_TIB_SEH)
	*--sp = 0; // gs::0x0, structured exception handling frame
#endif //#if USING(SAVE_TIB_SEH)
#if USING(SAVE_TIB_STACK)
	const uintptr_t stackBase = reinterpret_cast<uintptr_t>(spBase);
	const uintptr_t stackCeil = stackBase - stackSize;

	// pie
	*--sp = stackBase; // gs:0x8, stack base (high address)
	*--sp = stackCeil; // gs:0x10, stack ceiling (low address)
	*--sp = stackCeil; // gs:0x1478, true stack base. Can be used to add guard pages and stack reallocations
	*--sp = stackSize; // gs:0x1748, true stack size. Can be used by windows functions when reallocating stack with gs:1478
#endif //#if USING(SAVE_TIB_STACK)

	sp -= CPU_REG_COUNT;
	memset(sp, 0, CPU_REG_COUNT * sizeof(*sp));

	size_t fpuEntries = MOV_SIZE_ALIGNED / sizeof(*sp);
#if USING(SAVE_FPU_CONTROL)
	*--sp = _mm_getcsr();
	--fpuEntries;
#endif //#if USING(SAVE_FPU_CONTROL)

	sp -= fpuEntries;
	memset(sp, 0, fpuEntries * sizeof(*sp));

	return sp;
}
