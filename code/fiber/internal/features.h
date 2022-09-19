#pragma once

#include "usings.h"
#include "platform.h"

#define VALIDATE_FIBERS  USE_IF(VALIDATE)
#define SAVE_TIB_STACK   USE_IF(USING(OS_WINDOWS) && !IGNORE_OS_DATA)
#define SAVE_TIB_SEH     USE_IF(USING(OS_WINDOWS) && USING(PROC_X86) && !IGNORE_OS_DATA)
#define SAVE_FPU_CONTROL USE_IF(!FAST_FPU)

#if USING(SAVE_TIB_SEH)
static constexpr unsigned TIB_SEH_ENTRIES = 1; // gs:0h
#else //#if USING(SAVE_TIB_SEH)
static constexpr unsigned TIB_SEH_ENTRIES = 0;
#endif //#else //#if USING(SAVE_TIB_SEH)

#if USING(SAVE_TIB_STACK)
static constexpr unsigned TIB_STACK_ENTRIES = 4; // gs:8h, gs:10h, gs:1478h, gs:1748h
#else //#if USING(SAVE_TIB_STACK)
static constexpr unsigned TIB_STACK_ENTRIES = 0;
#endif //#else //#if USING(SAVE_TIB_STACK)

#if USING(SAVE_FPU_CONTROL)
static constexpr unsigned FPU_CONTROL_ENTRIES = 1; // mxscr or controlfp
#else //#if USING(SAVE_FPU_CONTROL)
static constexpr unsigned FPU_CONTROL_ENTRIES = 0;
#endif //#else //#if USING(SAVE_FPU_CONTROL)
