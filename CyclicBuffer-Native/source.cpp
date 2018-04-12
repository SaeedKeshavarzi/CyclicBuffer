#define _CRT_SECURE_NO_WARNINGS

#define IS_LOCK_FREE 1
#define IS_RECYCLABLE 1

#include <stdio.h>
#include <stdint.h>
#include <Windows.h>

#include "cyclic_buffer.h"

cyclic_buffer<int32_t, IS_LOCK_FREE, IS_RECYCLABLE> buffer(10);

DWORD WINAPI producer(LPVOID lpParam)
{
	for (int32_t cnt = 0; !buffer.is_terminated(); ++cnt)
	{
		buffer.push(cnt);
	}

	printf("producer say goodbye\n");
	return 0;
}

DWORD WINAPI consumer(LPVOID lpParam)
{
	int32_t cnt, curr, last(-1);

	buffer.wait_for_data();
	auto _1 = clock();

	for (cnt = 0; cnt < 100000000; ++cnt)
	{
#if IS_RECYCLABLE
		curr = buffer.pop(0);
#else
		curr = buffer.pop();
#endif

		if (curr < last)
		{
			printf("Error: %d : %d \n", cnt, curr);
		}

		last = curr;
	}

	auto _2 = clock();

	buffer.terminate();
	printf("%d : %d : %f\n", cnt - 1, curr, (double)curr / (cnt - 1));
	printf("%d ms\n", _2 - _1);

	return 0;
}

int main()
{
	HANDLE t2 = CreateThread(NULL, 0, consumer, NULL, 0, NULL);
	HANDLE t1 = CreateThread(NULL, 0, producer, NULL, 0, NULL);

	WaitForSingleObject(t1, INFINITE);
	WaitForSingleObject(t2, INFINITE);

	CloseHandle(t1);
	CloseHandle(t2);

	getchar();
	return 0;
}
