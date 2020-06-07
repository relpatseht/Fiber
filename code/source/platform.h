#pragma once

#if defined(_WIN32)
# if defined(_M_X64)
#  define CONTEXT_REG_COUNT 32 // xmm6-xmm15 (double wide) + 1 single align, rsi, rdi, rbp, rbx, r12-r15, gs:0, gs:8, gs:10 (TIB stack data)
# else //# if defined(_M_X64)
#  error Unsupported platform. Only x64 is currently supported on windows.
# endif //# else //# if defined(_M_X64)
#elif defined(__linux__) //#if defined(__WIN32)
# if defined(__x86_64__)
#  define CONTEXT_REG_COUNT 6 // rbp, rbx, r12-r15
# elif defined(__arm__) && !defined(__aarch64__) // # if defined(__x86_64__)
#  if __ARM_PCS_VFP
#   define CONTEXT_REG_COUNT 26 // r4-r12, lr (r14), d8-d15 (double wide)
#  else //#  if __ARM_PCS_VFP
#   define CONTEXT_REG_COUNT 10 // r4-r12, lr (r14)
#  endif //#  else //#  if __ARM_PCS_VFP
# elif defined(__aarch64__) //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  if __ARM_PCS_VFP
#  define CONTEXT_REG_COUNT 14 // x16,x17,x18, x19-x29, x30 (lr)
#  else //#  if __ARM_PCS_VFP
#  define CONTEXT_REG_COUNT 30 // x16,x17,x18, x19-x29, x30 (lr), d8-d15 (double wide)
#  endif //#  else //#  if __ARM_PCS_VFP
# else //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  error Unsupported platform. Only x64, arm, and arm64 are currently supported on linux.
# endif //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#else //#elif defined(__linux__) //#if defined(__WIN32)
# error Unsupported operating system. Only Windows and Linux currently supported.
#endif //#else //#elif defined(__linux__) //#if defined(__WIN32)