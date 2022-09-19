#pragma once

#include "usings.h"

#define OS_WINDOWS USE_IF(_WIN32)
#define OS_LINUX   USE_IF(false && __gnu_linux__)

#if !USING(OS_WINDOWS) && !USING(OS_LINUX)
# error Unsupported operating sytem. Only windows and linux are currently supported.
#endif //#if !USING(OS_WINDOWS) && !USING(OS_LINUX)

#define PROC_X64    USE_IF(_M_X64 || __x86_64__)
#define PROC_X86    USE_IF(false && !USING(PROC_X64) && (_M_IX86 || _X86_ || __i386__))
#define PROC_ARM64  USE_IF(false && (_M_ARM64 || __aarch64__))
#define PROC_ARM    USE_IF(false && !USING(PROC_ARM64) && (_M_ARM || __arm__))
#define PROC_ARM_FP USE_IF(USING(PROC_ARM) && __ARM_PCS_VFP)

#if !USING(PROC_X64) && !USING(PROC_X86) && !USING(PROC_ARM64) && !USING(PROC_ARM)
# error Unsupported architecture. Only x86, x64, arm, and arm64 are currently supported.
#endif //#if !USING(PROC_X64) && !USING(PROC_X86) && !USING(PROC_ARM64) && !USING(PROC_ARM)

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

#if defined(_WIN32)
# if defined(_M_X64)
#  if defined(NO_TIB_DATA)
#   if !defined(NO_TIB_STACK)
#    define NO_TIB_STACK
#   endif
#   if !defined(NO_TIB_SEH_FRAME)
#    define NO_TIB_SEH_FRAME
#   endif
#  endif
#  if !(defined(NO_TIB_STACK) || defined(NO_TIB_SEH_FRAME))
#   define EXTRA_QWORD_COUNT 5 // gs:0, gs:8, gs:10, gs:1478, gs:1748 (TIB SEH frame and stack data)
#  elif defined(NO_TIB_STACK)
#   define EXTRA_QWORD_COUNT 1 // gs:0 (TIB SEH frame)
#  elif defined(NO_TIB_SEH_FRAME)
#   define EXTRA_QWORD_COUNT 4 // gs:8, gs:10, gs:1478, gs1748 (TIB stack data)
#  endif
#  define BASE_REG_COUNT 8 // rsi, rdi, rbp, rbx, r12, r13, r14, r15
#  define XMM_REG_COUNT 10 // xmm[6,15]
# else //# if defined(_M_X64)
#  error Unsupported platform. Only x64 is currently supported on windows.
# endif //# else //# if defined(_M_X64)

#define TEXT_SECTION __declspec(allocate(".text"))
#elif defined(__linux__) //#if defined(__WIN32)
# if defined(__x86_64__)
#  define BASE_REG_COUNT 6 // rbp, rbx, r12-r15
# elif defined(__arm__) && !defined(__aarch64__) // # if defined(__x86_64__)
#  if __ARM_PCS_VFP
#   define XMM_REG_COUNT 8 // d8-d15 (double wide)
#  endif //# if __ARM_PCS_VFP
#  define BASE_REG_COUNT 10 // r4-r12, lr (r14)
# elif defined(__aarch64__) //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  if !__ARM_PCS_VFP
#   define XMM_REG_COUNT 8 // d8-d15 (double wide)
#  endif //#  if __ARM_PCS_VFP
#  define BASE_REG_COUNT 14 // x16,x17,x18, x19-x29, x30 (lr)
# else //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  error Unsupported platform. Only x64, arm, and arm64 are currently supported on linux.
# endif //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)

#define TEXT_SECTION __attribute__((section(".text#")))
#else //#elif defined(__linux__) //#if defined(__WIN32)
# error Unsupported operating system. Only Windows and Linux currently supported.
#endif //#else //#elif defined(__linux__) //#if defined(__WIN32)

#define PRESERVE_CSR 1

#pragma once

#if defined(_WIN32)
# if defined(_M_X64)
#  if defined(NO_TIB_DATA)
#   if !defined(NO_TIB_STACK)
#    define NO_TIB_STACK
#   endif
#   if !defined(NO_TIB_SEH_FRAME)
#    define NO_TIB_SEH_FRAME
#   endif
#  endif
#  if !(defined(NO_TIB_STACK) || defined(NO_TIB_SEH_FRAME))
#   define EXTRA_QWORD_COUNT 5 // gs:0, gs:8, gs:10, gs:1478, gs:1748 (TIB SEH frame and stack data)
#  elif defined(NO_TIB_STACK)
#   define EXTRA_QWORD_COUNT 1 // gs:0 (TIB SEH frame)
#  elif defined(NO_TIB_SEH_FRAME)
#   define EXTRA_QWORD_COUNT 4 // gs:8, gs:10, gs:1478, gs1748 (TIB stack data)
#  endif
#  define BASE_REG_COUNT 8 // rsi, rdi, rbp, rbx, r12, r13, r14, r15
#  define XMM_REG_COUNT 10 // xmm[6,15]
# else //# if defined(_M_X64)
#  error Unsupported platform. Only x64 is currently supported on windows.
# endif //# else //# if defined(_M_X64)

#define TEXT_SECTION __declspec(allocate(".text"))
#elif defined(__linux__) //#if defined(__WIN32)
# if defined(__x86_64__)
#  define BASE_REG_COUNT 6 // rbp, rbx, r12-r15
# elif defined(__arm__) && !defined(__aarch64__) // # if defined(__x86_64__)
#  if __ARM_PCS_VFP
#   define XMM_REG_COUNT 8 // d8-d15 (double wide)
#  endif //# if __ARM_PCS_VFP
#  define BASE_REG_COUNT 10 // r4-r12, lr (r14)
# elif defined(__aarch64__) //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  if !__ARM_PCS_VFP
#   define XMM_REG_COUNT 8 // d8-d15 (double wide)
#  endif //#  if __ARM_PCS_VFP
#  define BASE_REG_COUNT 14 // x16,x17,x18, x19-x29, x30 (lr)
# else //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  error Unsupported platform. Only x64, arm, and arm64 are currently supported on linux.
# endif //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)

#define TEXT_SECTION __attribute__((section(".text#")))
#else //#elif defined(__linux__) //#if defined(__WIN32)
# error Unsupported operating system. Only Windows and Linux currently supported.
#endif //#else //#elif defined(__linux__) //#if defined(__WIN32)
