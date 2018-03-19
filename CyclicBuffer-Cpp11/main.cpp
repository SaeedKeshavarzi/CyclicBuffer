#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <thread>

#include "cyclic_buffer.h"
#include "thread_naming.h"

cyclic_buffer<false> buffer(10, 8, 3, 3);
volatile bool is_finished{ false };

void producer()
{
	int32_t *data = new int32_t[1]{ 0 };
	int32_t cnt = 0;

	while (!is_finished)
	{
		buffer.push((char**)(&data));
		(*data) = ++cnt;
	}

	printf("producer say goodbye\n");
	delete[] data;
}

void consumer()
{
	int32_t *data = new int32_t[1], cnt;
	int32_t curr, last{ -1 };

	auto _1 = std::chrono::high_resolution_clock::now();

	for (cnt = 0; cnt < 100000000; ++cnt)
	{
		*data = -(1 + cnt);
		buffer.pop((char**)(&data));

		curr = *data;

		// if ((curr < 0) || (curr < last))
		if (curr != cnt)
		{
			printf("Error: %d : %d \n", cnt, *data);
		}

		last = curr;
	}

	auto _2 = std::chrono::high_resolution_clock::now();

	is_finished = true;
	printf("%d : %d : %f\n", cnt - 1, *data, (double)(*data) / (cnt - 1));

	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(_2 - _1).count();
	printf("%I64d ms\n", diff);

	buffer.terminate();
	delete[] data;
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
