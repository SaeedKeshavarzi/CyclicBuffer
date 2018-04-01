#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <Windows.h>

#include "spin_lock.h"
#include "resettable_event.h"
#include "hystersis_counter_lock.h"

template<typename _Ty, bool lock_free>
class cyclic_buffer;

template<typename _Ty>
class cyclic_buffer<_Ty, true>
{
public:
	static const bool is_lock_free = true;

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true> type;

private:
	const LONG capacity;
	const LONG unlock_threshold;
	const LONG overwriting_step;

	value_type **data, **write_point, **read_point, **last_point;
	value_type *write_packet, *read_packet;
	volatile LONG size;
	spin_lock sync;

	bool terminated;
	manual_reset_event read_enable;

public:
	cyclic_buffer(const cyclic_buffer&);
	cyclic_buffer& operator=(const cyclic_buffer&);

	cyclic_buffer(const LONG _capacity, const LONG _element_size,
		const LONG _unlock_threshold = 1, const LONG _overwriting_step = 1) :
	capacity(_capacity),
		unlock_threshold(_unlock_threshold ),
		overwriting_step(_overwriting_step)
	{
		data = new value_type*[_capacity];
		for (LONG i = 0; i < _capacity; ++i)
		{
			data[i] = new value_type[_element_size];
		}

		write_packet = new value_type[_element_size];
		read_packet = new value_type[_element_size];

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

		delete write_packet;
		delete read_packet;

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		read_enable.set();
	}

	inline void push(value_type ** const write_cache)
	{
		{
			value_type *aux = *write_point;
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

	inline void push()
	{
		this->push(&write_packet);
	}

	inline void pop(value_type ** const read_cache)
	{
		this->wait();

		sync.lock();
		{
			value_type *aux = *read_point;
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

	inline value_type** get_write_packet()
	{
		return &write_packet;
	}

	inline value_type** get_read_packet()
	{
		return &read_packet;
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

template<typename _Ty>
class cyclic_buffer<_Ty, false>
{
public:
	static const bool is_lock_free = false;

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false> type;

private:
	const LONG capacity;

	value_type **data, **write_point, **read_point, **last_point;
	value_type *write_packet, *read_packet;
	hystersis_counter_lock size;
	bool terminated;

public:
	cyclic_buffer(const cyclic_buffer&);
	cyclic_buffer& operator=(const cyclic_buffer&);

	cyclic_buffer(const LONG _capacity, const LONG _element_size,
		const LONG _unlock_threshold_down = 1, const LONG _unlock_threshold_up = 1) :
	capacity(_capacity),
		size(_capacity, _unlock_threshold_down, _unlock_threshold_up, 0)
	{
		data = new value_type*[_capacity];
		for (LONG i = 0; i < _capacity; ++i)
		{
			data[i] = new value_type[_element_size];
		}

		write_packet = new value_type[_element_size];
		read_packet = new value_type[_element_size];

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

		delete write_packet;
		delete read_packet;

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		size.terminate();
	}

	inline void push(value_type ** const write_cache)
	{
		size.wait_for_add();

		{
			value_type *aux = *write_point;
			*write_point = *write_cache;
			*write_cache = aux;
		}
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline void push()
	{
		this->push(&write_packet);
	}

	inline void pop(value_type ** const read_cache)
	{
		size.wait_for_sub();

		{
			value_type *aux = (value_type*)(*read_point);
			*read_point = *read_cache;
			*read_cache = aux;
		}
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

	inline value_type** get_write_packet()
	{
		return &write_packet;
	}

	inline value_type** get_read_packet()
	{
		return &read_packet;
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
