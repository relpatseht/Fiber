#pragma once

#include <type_traits>

namespace scheduler
{
	struct Scheduler;
	struct TaskHandle
	{
		TaskHandle();
		TaskHandle(const TaskHandle& rhs);
		TaskHandle(TaskHandle&& rhs);
		TaskHandle& operator=(const TaskHandle& rhs);
		TaskHandle& operator=(TaskHandle&& rhs);
		~TaskHandle();

	private:
		void* data;
	};

	namespace task
	{
		TaskHandle Create(void (*Task)(void*), const void* userData, size_t dataSize, size_t alignment = 0);
		TaskHandle Create_Stack(void (*Task)(void*), const void* userData); // Task had better finish before userData goes out of scope

		template<typename FuncT>
		TaskHandle Create(FuncT Task)
		{
			static_assert(std::is_trivially_copyable_v<FuncT>);

			return Create([](const void* userData)
			{
				reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
			}, &Task, sizeof(Task), alignof(FuncT));
		}

		template<typename FuncT>
		TaskHandle Create_Stack(FuncT Task)
		{
			return Create_Stack([](const void* userData)
			{
				reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
			}, &Task);
		}

		void Run(TaskHandle task, unsigned optThread = ~0u);
		void RunAndWait(TaskHandle task, unsigned optThread = ~0u);
		void Wait(TaskHandle task);
	}
}