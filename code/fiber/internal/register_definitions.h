#pragma once

#include "platform.h"

#ifdef _MSC_VER
# define CODE_SEG_START __pragma(section(".text"))
# define CODE_SEG_ATTR  __declspec(allocate(".text"))
#else //#ifdef _MSC_VER
# define CODE_SEG_START 
# define CODE_SEG_ATTR  __attribute__((section(".text#")))
#endif //#else //#ifdef _MSC_VER

#if USING(PROC_X64)
# if USING(OS_WINDOWS)
static constexpr unsigned CPU_REG_COUNT = 8; // rsi, rdi, rbp, rbx, r12-15
static constexpr unsigned FPU_REG_COUNT = 10; // xmm[6,15]
# elif USING(OS_LINUX) //# if USING(OS_WINDOWS)
static constexpr unsigned CPU_REG_COUNT = 6; // // rbp, rbx, r12-r15
static constexpr unsigned FPU_REG_COUNT = 0;
# endif //# elif USING(OS_LINUX) //# if USING(OS_WINDOWS)
static constexpr unsigned CPU_REG_WIDTH = 8;
static constexpr unsigned FPU_REG_WIDTH = 16;
#elif USING(PROC_X86) //#if USING(PROC_X64)
static constexpr unsigned CPU_REG_COUNT = 4; // esi, edi, ebp, ebx
static constexpr unsigned FPU_REG_COUNT = 8; // ST[0-7]
static constexpr unsigned CPU_REG_WIDTH = 4;
static constexpr unsigned FPU_REG_WIDTH = 8;
#elif USING(PROC_ARM) //#elif USING(PROC_X86) //#if USING(PROC_X64)
static constexpr unsigned CPU_REG_COUNT = 10; // r4-r12, lr (r14)
# if USING(PROC_ARM_FP)
static constexpr unsigned FPU_REG_COUNT = 8; // d8-d15 (double wide)
# else //# if USING(PROC_ARM_FP)
static constexpr unsigned FPU_REG_COUNT = 0;
#endif //# else //# if USING(PROC_ARM_FP)
static constexpr unsigned CPU_REG_WIDTH = 4;
static constexpr unsigned FPU_REG_WIDTH = 8;
#elif USING(PROC_ARM64) //#elif USING(PROC_ARM) //#elif USING(PROC_X86) //#if USING(PROC_X64)
static constexpr unsigned CPU_REG_COUNT = 14; // x16,x17,x18, x19-x29, x30 (lr)
static constexpr unsigned FPU_REG_COUNT = 8; // d8-d15 (double wide)
static constexpr unsigned CPU_REG_WIDTH = 8;
static constexpr unsigned FPU_REG_WIDTH = 8;
#endif //#elif USING(PROC_ARM64) //#elif USING(PROC_ARM) //#elif USING(PROC_X86) //#if USING(PROC_X64)

static constexpr unsigned STACK_ALIGN = CPU_REG_WIDTH > FPU_REG_WIDTH ? CPU_REG_WIDTH : FPU_REG_WIDTH;
