#pragma once

#include "platform.h"
#include <cstdint>
#include <xmmintrin.h>

namespace ctx
{
#if XMM_REG_COUNT
	union XMMContext
	{
		__m128 regs[XMM_REG_COUNT];
		struct
		{
# if defined(_WIN32)
			__m128 xmm6, xmm7, xmm8, xmm9, xmm10, xmm11, xmm12, xmm13, xmm14, xmm15;
# elif (defined(__arm__) && !defined(__aarch64__) && __ARM_PCS_VFP) || (defined(__aarch64__) && !__ARM_PCS_VFP)
			__m128 d8, d9, d10, d11, d12, d13, d14, d15;
# else
#  error "No known xmm registers for this platform"
# endif
		};
	};
	static_assert(sizeof(XMMContext) == XMM_REG_COUNT * sizeof(__m128));
#endif

#if EXTRA_QWORD_COUNT
	union ExtraContext
	{
		uint64_t regs[EXTRA_QWORD_COUNT];
		struct
		{
# if defined(_WIN32)
#  if !defined(NO_TIB_SEH_FRAME)
			union
			{
				uint64_t gs0;
				uint64_t sehFrame;
			};
#  endif
#  if !defined(NO_TIB_STACK)
			union
			{
				struct
				{
					uint64_t gs8, gs10;
				};
				struct
				{
					uint64_t stackBase, stackCeil;
				};
			};
#  endif
# else
#  error "No known extra data for this platform"
# endif
		};
	};
	static_assert(sizeof(ExtraContext) == EXTRA_QWORD_COUNT * sizeof(uint64_t));
#endif

	union RegContext
	{
		uint64_t regs[BASE_REG_COUNT];
		struct
		{
#if defined(_WIN32)
			uint64_t rsi, rdi, rbp, rbx, r12, r13, r14, r15;
# elif defined(__linux__) && defined(__x86_64__)
			uint64_t rbp, rbx, r12, r13, r14, r15;
# elif defined(__linux__) && defined(__arm__) && !defined(__aarch64__) // # if defined(__x86_64__)
			uint64_t r4, r5, r6, r7, r8, r9, r10, r11, r12;
			union
			{
				uint64_t lr, r14;
			};
# elif defined(__linux__) && defined(__aarch64__) //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
			uint64_t x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
			union
			{
				uint64_t lr, x30;
			};
# else //# elif defined(__arm__) && !defined(__aarch64__)// # if defined(__x86_64__)
#  error "No known base registers for this platform"
#endif
		};
	};
	static_assert(sizeof(RegContext) == BASE_REG_COUNT * sizeof(uint64_t));

	struct Context
	{
#if EXTRA_QWORD_COUNT
		ExtraContext extra;
#endif
		RegContext base;
#if (EXTRA_QWORD_COUNT + BASE_REG_COUNT) & 0x1
		uint64_t __padding;
#endif
#if XMM_REG_COUNT
		XMMContext xmm;
#endif
	};
}