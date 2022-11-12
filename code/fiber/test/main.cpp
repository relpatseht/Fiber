#include "../fiber/fiber.h"
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

fiber::FiberAPI s_fiberAPI;

struct FiberData
{
	unsigned numFibers;
	fiber::Fiber** fibers;
	unsigned value;
};

static void Func1(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func1 with fiberIndex %u\n", data->value);
	s_fiberAPI.Switch(data->fibers[0], data->fibers[1]);
	printf("In func1 with fiberIndex %u\n", data->value);
	s_fiberAPI.Switch(data->fibers[0], data->fibers[3]);
}

static void Func2(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func2 with fiberIndex %u\n", data->value);
	s_fiberAPI.Switch(data->fibers[1], data->fibers[2]);
	printf("In func2 with fiberIndex %u\n", data->value);
}

static void Func3(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func3 with fiberIndex %u\n", data->value);
	s_fiberAPI.Switch(data->fibers[2], data->fibers[0]);
}

static void Func4(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func4 with fiberIndex %u\n", data->value);
}

int main()
{
	constexpr unsigned pageSize = 4 * 1024;
	constexpr unsigned stackSize = pageSize * 4;
	fiber::FiberFunc fiberFuncs[] = { Func1, Func2, Func3, Func4 };
	constexpr unsigned numFibers = sizeof(fiberFuncs) / sizeof(fiberFuncs[0]);
	void* stackMemBase[numFibers];
	void* stackBase[numFibers];
	void* stackCeil[numFibers];
	fiber::Fiber *fibers[numFibers];
	FiberData data[numFibers];

	s_fiberAPI = fiber::GetAPI(fiber::Options::OS_API_SAFETY | fiber::Options::PRESERVE_FPU_CONTROL);

	for (unsigned fiberIndex = 0; fiberIndex < numFibers; ++fiberIndex)
	{
		// Protection from underflows and overflows
		stackMemBase[fiberIndex] = VirtualAlloc(nullptr, stackSize + pageSize * 2, MEM_RESERVE, PAGE_NOACCESS);
		stackBase[fiberIndex] = reinterpret_cast<uint8_t*>(stackMemBase[fiberIndex]) + pageSize;
		stackCeil[fiberIndex] = reinterpret_cast<uint8_t*>(stackBase[fiberIndex]) + stackSize;

		VirtualAlloc(stackBase[fiberIndex], stackSize, MEM_COMMIT, PAGE_READWRITE);

		data[fiberIndex].numFibers = numFibers;
		data[fiberIndex].fibers = fibers;
		data[fiberIndex].value = fiberIndex + 1;

		fibers[fiberIndex] = s_fiberAPI.Create(stackBase[fiberIndex], stackSize, 0, fiberFuncs[fiberIndex], &data[fiberIndex]);
	}
	
	s_fiberAPI.Start(fibers[0]);

	printf("Back to main\n");

	return 0;
}
