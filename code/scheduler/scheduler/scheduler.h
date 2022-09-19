#pragma once

namespace scheduler
{
	struct Scheduler;

	Scheduler* Create();
	void Destroy(Scheduler* sch);
	void SetDefault(Scheduler* sch);
}
