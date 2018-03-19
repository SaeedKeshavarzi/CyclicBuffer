#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <Windows.h>

#include "cyclic_buffer.h"

cyclic_buffer<false> buffer(10, 8, 3, 3);
volatile bool is_finished(false);

DWORD WINAPI producer(LPVOID lpParam)
{
	int *data = new int[1];
	int cnt = 0;

	data[0] = 0;
	while (!is_finished)
	{
		buffer.push((char**)(&data));
		(*data) = ++cnt;
	}

	printf("producer say goodbye\n");
	delete[] data;
	return 0;
}

DWORD WINAPI consumer(LPVOID lpParam)
{
	int *data = new int[1], cnt;
	int curr, last(-1);

	auto _1 = clock();

	for (cnt = 0; cnt < 100000000; ++cnt)
	{
		*data = -(1 + cnt);
		buffer.pop((char**)(&data));

		curr = *data;

		//if ((curr < 0) || (curr < last))
		if (curr != cnt)
		{
			printf("Error: %d : %d \n", cnt, *data);
		}

		last = curr;
	}

	auto _2 = clock();

	is_finished = true;
	printf("%d : %d : %f\n", cnt - 1, *data, (double)(*data) / (cnt - 1));
	printf("%d ms\n", _2 - _1);

	buffer.terminate();
	delete[] data;
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
