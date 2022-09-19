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
