#pragma once

namespace scheduler
{
	struct Scheduler;

	namespace task
	{
		unsigned Create(void (*Task)(void*), const void* userData, size_t dataSize, Scheduler* optSCH = nullptr);
		unsigned Create_Stack(void (*Task)(void*), const void* userData, Scheduler* optSCH = nullptr); // Task had better finish before userData goes out of scope

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

			return Create(&Delegate::Run, &Task, sizeof(Task), optSCH);
		}

		template<typename FuncT>
		unsigned Create_Stack(FuncT Task, Scheduler* optSCH)
		{
			struct Delegate
			{
				static void Run(const void* userData)
				{
					return reinterpret_cast<FuncT*>(const_cast<void*>(uesrData))();
				}
			};

			return Create_Stack(&Delegate::Run, &Task, optSCH);
		}

		void Run(unsigned task, unsigned optThread = ~0u, Scheduler* optSCH = nullptr);
		void RunAndWait(unsigned task, unsigned optThread = ~0u, Scheduler* optSCH = nullptr);
		void Wait(unsigned task, Scheduler* optSCH = nullptr);
	}
}