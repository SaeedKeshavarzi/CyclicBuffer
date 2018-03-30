#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <atomic>

#include "resettable_event.h"
#include "hystersis_counter_lock.h"

template<bool lock_free>
class cyclic_buffer;

template<>
class cyclic_buffer<true>
{
private:
	const int capacity;
	const int unlock_threshold;
	const int overwriting_step;

	char **data, **write_point, **last_point;
	char *write_packet, *read_packet;
	std::atomic_flag sync = ATOMIC_FLAG_INIT;
	std::atomic<char**> read_point;
	std::atomic<int> size;

	bool terminated;
	manual_reset_event read_enable;

public:
	static const bool is_lock_free{ true };
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity, const int _element_size,
		const int _unlock_threshold = 1, const int _overwriting_step = 1) :
		capacity{ _capacity },
		unlock_threshold{ _unlock_threshold },
		overwriting_step{ _overwriting_step }
	{
		data = new char*[_capacity];
		for (int i = 0; i < _capacity; ++i)
		{
			data[i] = new char[_element_size];
		}

		write_packet = new char[_element_size];
		read_packet = new char[_element_size];

		write_point = read_point = data;
		last_point = data + _capacity - 1;
		size = 0;

		terminated = false;
		read_enable.reset();
	}

	~cyclic_buffer()
	{
		if (!terminated)
		{
			terminate();
		}

		for (int i = 0; i < capacity; ++i)
		{
			delete[] data[i];
		}

		delete[] write_packet;
		delete[] read_packet;

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		read_enable.set();
	}

	inline void push(char ** const write_cache)
	{
		std::swap(*write_point, *write_cache);
		(write_point == last_point ? write_point = data : ++write_point);

		bool not_overflow{ true };
		if (read_point.load() == write_point)
		{
			while (sync.test_and_set(std::memory_order_acquire));

			if (read_point.load() == write_point)
			{
				read_point = data + ((read_point.load() - data) + overwriting_step) % capacity;
				size.fetch_sub(overwriting_step - 1);

				not_overflow = false;
			}

			sync.clear(std::memory_order_release);
		}

		if (not_overflow)
		{
			size.fetch_add(1);
		}

		if (!read_enable.is_set() && (size.load() >= unlock_threshold))
		{
			read_enable.set();
		}
	}

	inline void push()
	{
		this->push(&write_packet);
	}

	inline void pop(char ** const read_cache)
	{
		this->wait();

		while (sync.test_and_set(std::memory_order_acquire));
		std::swap(*read_point.load(), *read_cache);
		(read_point == last_point ? read_point = data : ++read_point);
		sync.clear(std::memory_order_release);

		if ((size.fetch_sub(1) == 1) && !terminated)
		{
			read_enable.reset();
		}
	}

	inline void pop()
	{
		this->pop(&read_packet);
	}

	inline void wait()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
	}

	inline char** get_write_packet()
	{
		return &write_packet;
	}

	inline char** get_read_packet()
	{
		return &read_packet;
	}

	inline int get_capacity() const
	{
		return capacity;
	}

	inline int get_size() const
	{
		return size.load();
	}
};

template<>
class cyclic_buffer<false>
{
private:
	const int capacity;

	char **data, **write_point, **read_point, **last_point;
	char *write_packet, *read_packet;
	hystersis_counter_lock size;
	bool terminated;

public:
	static const bool is_lock_free{ false };
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity, const int _element_size,
		const int _unlock_threshold_down = 1, const int _unlock_threshold_up = 1) :
		capacity{ _capacity },
		size{ _capacity, _unlock_threshold_down, _unlock_threshold_up, 0 }
	{
		data = new char*[_capacity];
		for (int i = 0; i < _capacity; ++i)
		{
			data[i] = new char[_element_size];
		}

		write_packet = new char[_element_size];
		read_packet = new char[_element_size];

		write_point = read_point = data;
		last_point = data + _capacity - 1;

		terminated = false;
	}

	~cyclic_buffer()
	{
		if (!terminated)
		{
			terminate();
		}

		for (int i = 0; i < capacity; ++i)
		{
			delete[] data[i];
		}

		delete[] write_packet;
		delete[] read_packet;

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		size.terminate();
	}

	inline void push(char ** const write_cache)
	{
		size.wait_for_add();

		std::swap(*write_point, *write_cache);
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline void push()
	{
		this->push(&write_packet);
	}

	inline void pop(char ** const read_cache)
	{
		size.wait_for_sub();

		std::swap(*read_point, *read_cache);
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();
	}

	inline void pop()
	{
		this->pop(&read_packet);
	}

	inline void wait_for_space()
	{
		size.wait_for_add();
	}

	inline void wait_for_data()
	{
		size.wait_for_sub();
	}

	inline char** get_write_packet()
	{
		return &write_packet;
	}

	inline char** get_read_packet()
	{
		return &read_packet;
	}

	inline int get_capacity() const
	{
		return capacity;
	}

	inline int get_size() const
	{
		return size.get_value();
	}
};

#endif // !_CYCLIC_BUFFER_H_
