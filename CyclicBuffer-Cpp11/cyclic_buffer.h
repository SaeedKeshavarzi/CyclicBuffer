#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <atomic>

#include "spin_lock.h"
#include "resettable_event.h"
#include "counter_lock.h"

template<typename _Ty, bool lock_free = false, bool recyclable = false>
class cyclic_buffer;

template<typename _Ty>
class cyclic_buffer<_Ty, true, true>
{
public:
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ true };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, true> type;

private:
	const int capacity;

	value_type *data, *write_point, *read_point, *last_point;
	std::atomic<int> size;
	spin_lock sync;

	bool terminated;
	manual_reset_event read_enable;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity) : capacity{ _capacity }
	{
		write_point = read_point = data = (value_type*)malloc(_capacity * sizeof(value_type));
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

		free(data);
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

	inline value_type* get_data() const
	{
		return data;
	}

	inline value_type push(const value_type& value)
	{
		value_type result = *write_point;
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		if (read_point == write_point)
		{
			sync.lock();

			if (read_point == write_point)
			{
				(read_point == last_point ? read_point = data : ++read_point);
				sync.unlock();
			}
			else
			{
				sync.unlock();
				size.fetch_add(1);
			}
		}
		else
		{
			size.fetch_add(1);
		}

		if (!read_enable.is_set() && (size.load() > 0))
		{
			read_enable.set();
		}

		return result;
	}

	inline value_type pop(const value_type& value)
	{
		value_type result;
		this->wait_for_data();

		sync.lock();
		result = *read_point;
		*read_point = value;
		(read_point == last_point ? read_point = data : ++read_point);
		sync.unlock();

		if ((size.fetch_sub(1) == 1) && !terminated)
		{
			read_enable.reset();
		}

		return result;
	}

	inline void wait_for_data()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
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
class cyclic_buffer<_Ty, true, false>
{
public:
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ false };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, false> type;

private:
	const int capacity;

	value_type *data, *write_point, *last_point;
	std::atomic<value_type*> read_point;
	std::atomic<int> size;

	bool terminated;
	manual_reset_event read_enable;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity) : capacity{ _capacity }
	{
		write_point = read_point = data = (value_type*)malloc(_capacity * sizeof(value_type));
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

		free(data);
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

	inline value_type* get_data() const
	{
		return data;
	}

	inline void push(const value_type& value)
	{
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		value_type* offset{ write_point };
		if (!read_point.compare_exchange_strong(offset, offset == last_point ? data : offset + 1))
		{
			size.fetch_add(1);
		}

		if (!read_enable.is_set() && (size.load() > 0))
		{
			read_enable.set();
		}
	}

	inline value_type pop()
	{
		this->wait_for_data();

		value_type result, *offset{ read_point.load() };
		do {
			result = *offset;
		} while (!read_point.compare_exchange_weak(offset, offset == last_point ? data : offset + 1));

		if ((size.fetch_sub(1) == 1) && !terminated)
		{
			read_enable.reset();
		}

		return result;
	}

	inline void wait_for_data()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
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
class cyclic_buffer<_Ty, false, true>
{
public:
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ true };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, true> type;

private:
	const int capacity;

	value_type *data, *write_point, *read_point, *last_point;
	counter_lock size;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity) : capacity{ _capacity }, size{ _capacity , 0 }
	{
		write_point = read_point = data = (value_type*)malloc(_capacity * sizeof(value_type));
		last_point = data + _capacity - 1;
	}

	~cyclic_buffer()
	{
		if (!size.is_terminated())
		{
			size.terminate();
		}

		free(data);
	}

	inline void terminate()
	{
		size.terminate();
	}

	inline bool is_terminated() const
	{
		return size.is_terminated();
	}

	inline value_type* get_data() const
	{
		return data;
	}

	inline value_type push(const value_type& value)
	{
		this->wait_for_space();

		value_type result = *write_point;
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();

		return result;
	}

	inline value_type pop(const value_type& value)
	{
		this->wait_for_data();

		value_type result = *read_point;
		*read_point = value;
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();

		return result;
	}

	inline void wait_for_space()
	{
		if (size.get_value() == capacity)
		{
			size.wait_for_add();
		}
	}

	inline void wait_for_data()
	{
		if (size.get_value() == 0)
		{
			size.wait_for_sub();
		}
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

template<typename _Ty>
class cyclic_buffer<_Ty, false, false>
{
public:
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ false };

	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, false> type;

private:
	const int capacity;

	value_type *data, *write_point, *read_point, *last_point;
	counter_lock size;

public:
	cyclic_buffer(const cyclic_buffer&) = delete;
	cyclic_buffer& operator=(const cyclic_buffer&) = delete;

	cyclic_buffer(const int _capacity) : capacity{ _capacity }, size{ _capacity , 0 }
	{
		write_point = read_point = data = (value_type*)malloc(_capacity * sizeof(value_type));
		last_point = data + _capacity - 1;
	}

	~cyclic_buffer()
	{
		if (!size.is_terminated())
		{
			size.terminate();
		}

		free(data);
	}

	inline void terminate()
	{
		size.terminate();
	}

	inline bool is_terminated() const
	{
		return size.is_terminated();
	}

	inline value_type* get_data() const
	{
		return data;
	}

	inline void push(const value_type& value)
	{
		this->wait_for_space();

		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline value_type pop()
	{
		this->wait_for_data();

		value_type result = *read_point;
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();

		return result;
	}

	inline void wait_for_space()
	{
		if (size.get_value() == capacity)
		{
			size.wait_for_add();
		}
	}

	inline void wait_for_data()
	{
		if (size.get_value() == 0)
		{
			size.wait_for_sub();
		}
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
