#define _CRT_SECURE_NO_WARNINGS

#define IS_LOCK_FREE true
#define IS_RECYCLABLE false

#include <stdio.h>
#include <thread>

#include "cyclic_buffer.h"
#include "thread_naming.h"

cyclic_buffer<int32_t, IS_LOCK_FREE, IS_RECYCLABLE> buffer(10);

void producer()
{
	for (int32_t cnt = 0; !buffer.is_terminated(); ++cnt)
	{
		buffer.push(cnt);
	}

	printf("producer say goodbye\n");
}

void consumer()
{
	int32_t cnt, curr, last{ -1 };

	buffer.wait_for_data();
	auto _1{ std::chrono::high_resolution_clock::now() };

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

	auto _2{ std::chrono::high_resolution_clock::now() };

	buffer.terminate();
	printf("%d : %d : %f\n", cnt - 1, curr, (double)curr / (cnt - 1));

	auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(_2 - _1).count();
	printf("%I64d ms\n", diff);
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
