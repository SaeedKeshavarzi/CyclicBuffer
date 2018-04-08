//#define EXTERNAL_PACKET
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <thread>

#include "cyclic_buffer.h"
#include "thread_naming.h"

cyclic_buffer<int32_t, true> buffer(10, 1, 3, 3);
volatile bool is_finished{ false };

void producer()
{
#ifdef EXTERNAL_PACKET
	int32_t *data = new int32_t();

	for (int32_t cnt = 0; !is_finished; ++cnt)
	{
		*data = cnt;
		buffer.push(&data);
	}

	delete data;
#else
	for (int32_t cnt = 0; !is_finished; ++cnt)
	{
		*buffer.get_write_packet() = cnt;
		buffer.push();
	}
#endif // EXTERNAL_PACKET

	printf("producer say goodbye\n");
}

void consumer()
{
#ifdef EXTERNAL_PACKET
	int32_t *data{ new int32_t() };
#else
	int32_t *data{ buffer.get_read_packet() };
#endif // EXTERNAL_PACKET

	int32_t cnt, curr, last{ -1 };

	auto _1{ std::chrono::high_resolution_clock::now() };

	for (cnt = 0; cnt < 100000000; ++cnt)
	{
		*data = -(1 + cnt);

#ifdef EXTERNAL_PACKET
		buffer.pop(&data);
#else
		buffer.pop();
		data = buffer.get_read_packet();
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

	auto _2{ std::chrono::high_resolution_clock::now() };

	is_finished = true;
	printf("%d : %d : %f\n", cnt - 1, *data, (double)(*data) / (cnt - 1));

	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(_2 - _1).count();
	printf("%I64d ms\n", diff);

	buffer.terminate();

#ifdef EXTERNAL_PACKET
	delete data;
#endif // EXTERNAL_PACKET
}

int main()
{
	set_current_thread_name("0 main");

	std::thread t2(consumer);
	set_thread_name(t2, "1 consumer");

	std::thread t1(producer);
	set_thread_name(t1, "1 producer");

	t1.join();
	t2.join();

	getchar();
	return 0;
}
