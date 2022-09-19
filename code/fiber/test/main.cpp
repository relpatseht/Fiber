#include "../fiber/fiber.h"
#include <cstdio>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

struct FiberData
{
	unsigned numFibers;
	fiber::Fiber* fibers;
	unsigned value;
};

static void Func1(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func1 with fiberIndex %u\n", data->value);
	fiber::Switch(&data->fibers[0], &data->fibers[1]);
	printf("In func1 with fiberIndex %u\n", data->value);
	fiber::Switch(&data->fibers[0], &data->fibers[3]);
}

static void Func2(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func2 with fiberIndex %u\n", data->value);
	fiber::Switch(&data->fibers[1], &data->fibers[2]);
	printf("In func2 with fiberIndex %u\n", data->value);
}

static void Func3(void* dataPtr)
{
	const FiberData* const data = reinterpret_cast<FiberData*>(dataPtr);

	printf("In func3 with fiberIndex %u\n", data->value);
	fiber::Switch(&data->fibers[2], &data->fibers[0]);
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
	fiber::Fiber fibers[numFibers];
	FiberData data[numFibers];

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

		fibers[fiberIndex] = fiber::Create(stackBase[fiberIndex], stackSize, fiberFuncs[fiberIndex], &data[fiberIndex]);
	}
	
	fiber::Start(&fibers[0]);

	printf("Back to main\n");

	return 0;
}

//LoadContextASM:
//58 
//65 ff 34 25 00 00 00 00 
//65 ff 34 25 08 00 00 00 
//65 ff 34 25 10 00 00 00 
//56 
//57 
//55 
//53 
//41 54 
//41 55 
//41 56 
//41 57 
//48 81 ec a8 00 00 00 
//0f 29 b4 24 98 00 00 00 
//0f 29 bc 24 88 00 00 00 
//44 0f 29 44 24 78 
//44 0f 29 4c 24 68 
//44 0f 29 54 24 58 
//44 0f 29 5c 24 48 
//44 0f 29 64 24 38 
//44 0f 29 6c 24 28 
//44 0f 29 74 24 18 
//44 0f 29 7c 24 08 
//0f ae 1c 24 
//ff e0 
//StartFiberASM:
//59 
//58 
//ff d0 
//EndFiberASM:
//48 83 c4 28 
//5c 
//LoadContextASM:
//0f ae 14 24 
//44 0f 28 7c 24 08 
//44 0f 28 74 24 18 
//44 0f 28 6c 24 28 
//44 0f 28 64 24 38 
//44 0f 28 5c 24 48 
//44 0f 28 54 24 58 
//44 0f 28 4c 24 68 
//44 0f 28 44 24 78 
//0f 28 bc 24 88 00 00 00 
//0f 28 b4 24 98 00 00 00 
//48 81 c4 a8 00 00 00 
//41 5f 
//41 5e 
//41 5d 
//41 5c 
//5b 
//5d 
//5f 
//5e 
//65 8f 04 25 10 00 00 00 
//65 8f 04 25 08 00 00 00 
//65 8f 04 25 00 00 00 00 
//c3 
//SwitchToFiberASM
//e8 10 ff ff ff 
//48 89 21 
//48 8b 22 
//eb 83 
//StartFiberASM
//e8 03 ff ff ff 
//48 89 a1 40 01 00 00 
//48 8b e1 
//e9 6f ff ff ff 
//48 8d 05 5f ff ff ff 
//c3
