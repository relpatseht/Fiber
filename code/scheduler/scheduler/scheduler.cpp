#include "platform.h"
#include "scheduler.h"
#include "thread.h"
#include "task.h"

#include "fiber.h"

#include <vector>
#include <thread>
#include <unordered_map>

namespace
{
	static constexpr unsigned THREAD_WAIT_QUEUE_SIZE = 32;

	struct Task
	{
		Task* next;
		void (*TaskFunc)(void*);

	};

	struct Thread
	{
		std::thread thread;
		spsc::queue 
	};
}

namespace scheduler
{
	struct Scheduler
	{
		fiber::FiberAPI fiberAPI;
		std::unordered_map<unsigned, unsigned> threadIdToIndex;
	};
}

namespace scheduler
{

	namespace thread
	{

	}

	namespace task
	{

	}

	Scheduler* Create(Options opts)
	{

	}

	void Destroy(Scheduler* sch)
	{

	}

	void SetDefault(Scheduler* sch)
	{

	}
}