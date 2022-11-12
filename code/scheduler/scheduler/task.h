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
		TaskHandle Create(FuncT Task, Scheduler* optSCH)
		{
			struct Delegate
			{
				static void Run(const void* userData)
				{
					return reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
				}
			};
			static_assert(std::is_trivially_copyable_v<FuncT>);

			return Create(&Delegate::Run, &Task, sizeof(Task), alignof(FuncT));
		}

		template<typename FuncT>
		TaskHandle Create_Stack(FuncT Task)
		{
			struct Delegate
			{
				static void Run(const void* userData)
				{
					return reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
				}
			};
			return Create_Stack(&Delegate::Run, &Task);
		}

		void Run(TaskHandle task, unsigned optThread = ~0u);
		void RunAndWait(TaskHandle task, unsigned optThread = ~0u);
		void Wait(TaskHandle task);
	}
}