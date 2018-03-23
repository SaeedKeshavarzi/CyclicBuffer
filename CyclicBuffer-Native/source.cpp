#define EXTERNAL_PACKET
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <Windows.h>

#include "cyclic_buffer.h"

cyclic_buffer<false> buffer(10, 8, 3, 3);
volatile bool is_finished(false);

DWORD WINAPI producer(LPVOID lpParam)
{
#ifdef EXTERNAL_PACKET
	int32_t *data = new int32_t[1];

	for (int32_t cnt = 0; !is_finished; ++cnt)
	{
		*data = cnt;
		buffer.push((char**)(&data));
	}

	delete[] data;
#else
	for (int32_t cnt = 0; !is_finished; ++cnt)
	{
		**(int32_t**)buffer.get_write_packet() = cnt;
		buffer.push();
	}
#endif // EXTERNAL_PACKET

	printf("producer say goodbye\n");
	return 0;
}

DWORD WINAPI consumer(LPVOID lpParam)
{
#ifdef EXTERNAL_PACKET
	int32_t *data( new int32_t[1] );
#else
	int32_t *data( *(int32_t**)buffer.get_read_packet() );
#endif // EXTERNAL_PACKET

	int32_t cnt, curr, last( -1 );

	auto _1 = clock();

	for (cnt = 0; cnt < 100000000; ++cnt)
	{
		*data = -(1 + cnt);

#ifdef EXTERNAL_PACKET
		buffer.pop((char**)(&data));
#else
		buffer.pop();
		data = *(int32_t**)buffer.get_read_packet();
#endif // EXTERNAL_PACKET

		curr = *data;

		if (buffer.is_lock_free)
		{
			if ((curr < 0) || (curr < last))
			{
				printf("Error: %d : %d \n", cnt, *data);
			}
		}
		else if (curr != cnt)
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

#ifdef EXTERNAL_PACKET
	delete[] data;
#endif // EXTERNAL_PACKET
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
