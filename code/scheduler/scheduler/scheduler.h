#pragma once

namespace scheduler
{
	struct Scheduler;

	enum class Options : unsigned
	{
		NONE = 0,
		OS_ABI_SAFE = 1<<0,
		PRESERVE_FPU_CONTROL = 1<<1,
		WORK_STEALING = 1<<2,
	};

	constexpr Options operator|(Options a, Options b)
	{
		return static_cast<Options>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
	}

	Scheduler* Create(Options opts);
	void Destroy(Scheduler* sch);
	void SetDefault(Scheduler* sch);
}
