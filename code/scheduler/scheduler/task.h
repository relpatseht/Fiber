#pragma once

namespace scheduler
{
	struct Scheduler;

	namespace task
	{
		unsigned Create(void (*Task)(void*), const void* userData, size_t dataSize, size_t alignment = 0);
		unsigned Create_Stack(void (*Task)(void*), const void* userData); // Task had better finish before userData goes out of scope

		template<typename FuncT>
		unsigned Create(FuncT Task, Scheduler* optSCH)
		{
			struct Delegate
			{
				static void Run(const void* userData)
				{
					return reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
				}
			};
			static_assert(std::is_trivially_copyable_v<T>);

			return Create(&Delegate::Run, &Task, sizeof(Task), alignof(FuncT));
		}

		template<typename FuncT>
		unsigned Create_Stack(FuncT Task)
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

		void Run(unsigned task, unsigned optThread = ~0u);
		void RunAndWait(unsigned task, unsigned optThread = ~0u);
		void Wait(unsigned task);
	}
}