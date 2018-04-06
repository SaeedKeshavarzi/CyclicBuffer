#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <atomic>

#include "resettable_event.h"
#include "hystersis_counter_lock.h"

template<typename _Ty, bool lock_free>
class cyclic_buffer;

template<typename _Ty>
class cyclic_buffer<_Ty, true>
{
public:
	static constexpr bool is_lock_free{ true };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true> type;

private:
	const int capacity;
	const int element_size;
	const int unlock_threshold;
	const int overwriting_step;

	value_type **data, **write_point, **last_point;
	value_type *write_packet, *read_packet;
	std::atomic_flag sync = ATOMIC_FLAG_INIT;
	std::atomic<value_type**> read_point;
	std::atomic<int> size;

	bool terminated;
	manual_reset_event read_enable;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity, const int _element_size = 1,
		const int _unlock_threshold = 1, const int _overwriting_step = 1) :
		capacity{ _capacity },
		element_size{ _element_size },
		unlock_threshold{ _unlock_threshold },
		overwriting_step{ _overwriting_step },
		size{ 0 }
	{
		data = new value_type*[_capacity];

		if (_element_size == 1)
		{
			for (int i = 0; i < _capacity; ++i)
			{
				data[i] = new value_type();
			}

			write_packet = new value_type();
			read_packet = new value_type();
		}
		else
		{
			for (int i = 0; i < _capacity; ++i)
			{
				data[i] = new value_type[_element_size];
			}

			write_packet = new value_type[_element_size];
			read_packet = new value_type[_element_size];
		}

		write_point = read_point = data;
		last_point = data + _capacity - 1;

		terminated = false;
		read_enable.reset();
	}

	~cyclic_buffer()
	{
		if (!terminated)
		{
			terminate();
		}

		if (element_size == 1)
		{
			for (int i = 0; i < capacity; ++i)
			{
				delete data[i];
			}

			delete write_packet;
			delete read_packet;
		}
		else
		{
			for (int i = 0; i < capacity; ++i)
			{
				delete[] data[i];
			}

			delete[] write_packet;
			delete[] read_packet;
		}

		delete[] data;
	}

	inline void terminate()
	{
		terminated = true;
		read_enable.set();
	}

	inline bool is_terminated() const
	{
		return terminated;
	}

	inline void push(value_type ** const write_cache)
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

	inline void pop(value_type ** const read_cache)
	{
		if (!this->wait())
		{
			return;
		}

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

	inline bool wait()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}

		return !terminated;
	}

	inline value_type** get_write_packet()
	{
		return &write_packet;
	}

	inline value_type** get_read_packet()
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

template<typename _Ty>
class cyclic_buffer<_Ty, false>
{
public:
	static constexpr bool is_lock_free{ false };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false> type;

private:
	const int capacity;
	const int element_size;

	value_type **data, **write_point, **read_point, **last_point;
	value_type *write_packet, *read_packet;
	hystersis_counter_lock size;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity, const int _element_size = 1,
		const int _unlock_threshold_down = 1, const int _unlock_threshold_up = 1) :
		capacity{ _capacity },
		element_size{ _element_size },
		size{ _capacity, _unlock_threshold_down, _unlock_threshold_up, 0 }
	{
		data = new value_type*[_capacity];

		if (_element_size == 1)
		{
			for (int i = 0; i < _capacity; ++i)
			{
				data[i] = new value_type();
			}

			write_packet = new value_type();
			read_packet = new value_type();
		}
		else
		{
			for (int i = 0; i < _capacity; ++i)
			{
				data[i] = new value_type[_element_size];
			}

			write_packet = new value_type[_element_size];
			read_packet = new value_type[_element_size];
		}

		write_point = read_point = data;
		last_point = data + _capacity - 1;
	}

	~cyclic_buffer()
	{
		if (!size.is_terminated())
		{
			size.terminate();
		}

		if (element_size == 1)
		{
			for (int i = 0; i < capacity; ++i)
			{
				delete data[i];
			}

			delete write_packet;
			delete read_packet;
		}
		else
		{
			for (int i = 0; i < capacity; ++i)
			{
				delete[] data[i];
			}

			delete[] write_packet;
			delete[] read_packet;
		}

		delete[] data;
	}

	inline void terminate()
	{
		size.terminate();
	}

	inline bool is_terminated() const
	{
		return size.is_terminated();
	}

	inline void push(value_type ** const write_cache)
	{
		if (!size.wait_for_add())
		{
			return;
		}

		std::swap(*write_point, *write_cache);
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline void push()
	{
		this->push(&write_packet);
	}

	inline void pop(value_type ** const read_cache)
	{
		if (!size.wait_for_sub())
		{
			return;
		}

		std::swap(*read_point, *read_cache);
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();
	}

	inline void pop()
	{
		this->pop(&read_packet);
	}

	inline bool wait_for_space()
	{
		return size.wait_for_add();
	}

	inline bool wait_for_data()
	{
		return size.wait_for_sub();
	}

	inline value_type** get_write_packet()
	{
		return &write_packet;
	}

	inline value_type** get_read_packet()
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
