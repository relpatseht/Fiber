#pragma once

namespace scheduler
{
	struct Scheduler;

	namespace thread
	{
		unsigned Add(Scheduler* optSCH = nullptr);
		void Detach(unsigned threadId, Scheduler* optSCH = nullptr); // Thread will finish it's tasks and die. No further tasks added
		void Destroy(unsigned threadId, Scheduler* optSCH = nullptr); // Thread will finish current task and die. Other tasks will be pushed to remaining threads
		bool Pin(unsigned threadId, unsigned core, Scheduler* optSCH = nullptr);
	}
}