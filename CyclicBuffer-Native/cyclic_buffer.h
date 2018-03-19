#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <Windows.h>

#include "spin_lock.h"
#include "resettable_event.h"
#include "hystersis_counter_lock.h"

template<bool lock_free>
class cyclic_buffer;

template<>
class cyclic_buffer<true>
{
private:
	const LONG capacity;
	const LONG unlock_threshold;
	const LONG overwriting_step;

	char **data, **write_point, **read_point, **last_point;
	volatile LONG size;
	spin_lock sync;

	bool terminated;
	manual_reset_event read_enable;

public:
	explicit cyclic_buffer(const LONG _capacity, const LONG _element_size,
		const LONG _unlock_threshold = 1, const LONG _overwriting_step = 1) :
	capacity(_capacity),
		unlock_threshold(_unlock_threshold ),
		overwriting_step(_overwriting_step)
	{
		data = new char*[_capacity];
		for (LONG i = 0; i < _capacity; ++i)
		{
			data[i] = new char[_element_size];
		}

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

		for (LONG i = 0; i < capacity; ++i)
		{
			delete[] data[i];
		}

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		read_enable.set();
	}

	inline void push(char ** const write_cache)
	{
		{
			char *aux = *write_point;
			*write_point = *write_cache;
			*write_cache = aux;
		}
		(write_point == last_point ? write_point = data : ++write_point);

		bool not_overflow(true);
		if (read_point == write_point)
		{
			sync.lock();

			if (read_point == write_point)
			{
				read_point = data + ((read_point - data) + overwriting_step) % capacity;

				{
					LONG copy;
					do
					{
						copy = size;
					} while (InterlockedCompareExchange(&size, copy - overwriting_step + 1, copy) != copy);
				}

				not_overflow = false;
			}

			sync.unlock();
		}

		if (not_overflow)
		{
			InterlockedIncrement(&size);
		}

		if (!read_enable.is_set() && (size >= unlock_threshold))
		{
			read_enable.set();
		}
	}

	inline void pop(char ** const read_cache)
	{
		this->wait();

		sync.lock();
		{
			char *aux = *read_point;
			*read_point = *read_cache;
			*read_cache = aux;
		}
		(read_point == last_point ? read_point = data : ++read_point);
		sync.unlock();

		if ((InterlockedDecrement(&size) == 1) && !terminated)
		{
			read_enable.reset();
		}
	}

	inline void wait()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
	}

	inline LONG get_capacity() const
	{
		return capacity;
	}

	inline LONG get_size() const
	{
		return size;
	}
};

template<>
class cyclic_buffer<false>
{
private:
	const LONG capacity;

	char **data, **write_point, **read_point, **last_point;
	hystersis_counter_lock size;
	bool terminated;

public:
	explicit cyclic_buffer(const LONG _capacity, const LONG _element_size,
		const LONG _unlock_threshold_down = 1, const LONG _unlock_threshold_up = 1) :
	capacity(_capacity),
		size(_capacity, _unlock_threshold_down, _unlock_threshold_up, 0)
	{
		data = new char*[_capacity];
		for (LONG i = 0; i < _capacity; ++i)
		{
			data[i] = new char[_element_size];
		}

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

		for (LONG i = 0; i < capacity; ++i)
		{
			delete[] data[i];
		}

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

		{
			char *aux = *write_point;
			*write_point = *write_cache;
			*write_cache = aux;
		}
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline void pop(char ** const read_cache)
	{
		size.wait_for_sub();

		{
			char *aux = (char*)(*read_point);
			*read_point = *read_cache;
			*read_cache = aux;
		}
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();
	}

	inline void wait_for_space()
	{
		size.wait_for_add();
	}

	inline void wait_for_data()
	{
		size.wait_for_sub();
	}

	inline LONG get_capacity() const
	{
		return capacity;
	}

	inline LONG get_size() const
	{
		return size.get_value();
	}
};

#endif // !_CYCLIC_BUFFER_H_
