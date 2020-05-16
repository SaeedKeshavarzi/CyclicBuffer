#ifndef _CYCLIC_BUFFER_H_
#define _CYCLIC_BUFFER_H_

#include <atomic>
#include <assert.h>

#include "resettable_event.h"
#include "counter_lock.h"
#include "spin_lock.h"

template<typename _Ty, bool _LockFree = false, bool _Recyclable = false>
class cyclic_buffer;

template<typename _Ty>
class cyclic_buffer<_Ty, true, true>
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, true> type;
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ true };

private:
	value_type * const data;
	value_type * const last_point;
	value_type * write_point;
	value_type * read_point;

	std::atomic<std::size_t> size;
	const std::size_t capacity;
	spin_lock guard;

	manual_reset_event read_enable;
	bool terminated;

public:
	cyclic_buffer(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_buffer(const std::size_t & _capacity) :
		capacity{ _capacity },
		data{ (value_type*)malloc((1 + _capacity) * sizeof(value_type)) },
		last_point{ data + _capacity }
	{
		assert(_capacity > (std::size_t)1);

		write_point = read_point = data;
		size = 0;

		terminated = false;
		read_enable.reset();
	}

	~cyclic_buffer()
	{
		if (!terminated)
			terminate();

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

	inline value_type * begin() const
	{
		return data;
	}

	inline value_type * end() const
	{
		return data + (1 + capacity);
	}

	inline value_type push(const value_type & value)
	{
		value_type result{ *write_point };
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		if (read_point == write_point)
		{
			guard.lock();

			if (read_point == write_point)
			{
				(read_point == last_point ? read_point = data : ++read_point);
				guard.unlock();
			}
			else
			{
				guard.unlock();
				size.fetch_add(1);
			}
		}
		else
			size.fetch_add(1);

		if (!read_enable.is_set() && (size.load() > 0))
			read_enable.set();

		return result;
	}

	inline value_type pop(const value_type & value)
	{
		this->wait_for_data();

		guard.lock();
		value_type result{ *read_point };
		*read_point = value;
		(read_point == last_point ? read_point = data : ++read_point);
		guard.unlock();

		if ((size.fetch_sub(1) == 1) && !terminated)
			read_enable.reset();

		return result;
	}

	inline const value_type operator[](const std::size_t & _index) const
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline value_type & operator[](const std::size_t & _index)
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline void wait_for_data() const
	{
		if (!read_enable.is_set() && !terminated)
			read_enable.wait();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_data_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (!read_enable.is_set() && !terminated)
			return read_enable.wait_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_data_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (!read_enable.is_set() && !terminated)
			return read_enable.wait_until(timeout_time);

		return true;
	}

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size.load();
	}
};

template<typename _Ty>
class cyclic_buffer<_Ty, true, false>
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, false> type;
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ false };

private:
	value_type * const data;
	value_type * const last_point;
	value_type * write_point;
	std::atomic<value_type*> read_point;

	std::atomic<std::size_t> size;
	const std::size_t capacity;

	manual_reset_event read_enable;
	bool terminated;

public:
	cyclic_buffer(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_buffer(const std::size_t & _capacity) :
		capacity{ _capacity },
		data{ (value_type*)malloc((1 + _capacity) * sizeof(value_type)) },
		last_point{ data + _capacity }
	{
		assert(_capacity > (std::size_t)1);

		write_point = read_point = data;
		size = 0;

		terminated = false;
		read_enable.reset();
	}

	~cyclic_buffer()
	{
		if (!terminated)
			terminate();

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

	inline value_type * begin() const
	{
		return data;
	}

	inline value_type * end() const
	{
		return data + (1 + capacity);
	}

	inline void push(const value_type & value)
	{
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		value_type * offset{ write_point };
		if (!read_point.compare_exchange_strong(offset, offset == last_point ? data : offset + 1))
			size.fetch_add(1);

		if (!read_enable.is_set() && (size.load() > 0))
			read_enable.set();
	}

	inline value_type pop()
	{
		this->wait_for_data();

		value_type result, *offset{ read_point.load() };
		do {
			result = *offset;
		} while (!read_point.compare_exchange_weak(offset, offset == last_point ? data : offset + 1));

		if ((size.fetch_sub(1) == 1) && !terminated)
			read_enable.reset();

		return result;
	}

	inline const value_type operator[](const std::size_t & _index) const
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline value_type & operator[](const std::size_t & _index)
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline void wait_for_data() const
	{
		if (!read_enable.is_set() && !terminated)
			read_enable.wait();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_data_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (!read_enable.is_set() && !terminated)
			return read_enable.wait_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_data_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (!read_enable.is_set() && !terminated)
			return read_enable.wait_until(timeout_time);

		return true;
	}

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size.load();
	}
};

template<typename _Ty>
class cyclic_buffer<_Ty, false, true>
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, true> type;
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ true };

private:
	value_type * const data;
	value_type * const last_point;
	value_type * write_point;
	value_type * read_point;

	counter_lock size;
	const std::size_t capacity;

public:
	cyclic_buffer(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_buffer(const std::size_t & _capacity) :
		capacity{ _capacity },
		size{ _capacity , 0 },
		data{ (value_type*)malloc(_capacity * sizeof(value_type)) },
		last_point{ data + _capacity - 1 }
	{
		assert(_capacity > (std::size_t)1);

		write_point = read_point = data;
	}

	~cyclic_buffer()
	{
		if (!size.is_terminated())
			size.terminate();

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

	inline value_type * begin() const
	{
		return data;
	}

	inline value_type * end() const
	{
		return data + capacity;
	}

	inline value_type push(const value_type & value)
	{
		this->wait_for_space();

		value_type result{ *write_point };
		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();

		return result;
	}

	inline value_type pop(const value_type & value)
	{
		this->wait_for_data();

		value_type result{ *read_point };
		*read_point = value;
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();

		return result;
	}

	inline const value_type operator[](const std::size_t & _index) const
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline value_type & operator[](const std::size_t & _index)
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline void wait_for_space() const
	{
		if (size.get_value() == capacity)
			size.wait_for_add();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_space_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (size.get_value() == capacity)
			return size.wait_for_add_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_space_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (size.get_value() == capacity)
			return size.wait_for_add_until(timeout_time);

		return true;
	}

	inline void wait_for_data() const
	{
		if (size.get_value() == 0)
			size.wait_for_sub();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_data_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (size.get_value() == 0)
			return size.wait_for_sub_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_data_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (size.get_value() == 0)
			return size.wait_for_sub_until(timeout_time);

		return true;
	}

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size.get_value();
	}
};

template<typename _Ty>
class cyclic_buffer<_Ty, false, false>
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, false> type;
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ false };

private:
	value_type * const data;
	value_type * const last_point;
	value_type * write_point;
	value_type * read_point;

	counter_lock size;
	const std::size_t capacity;

public:
	cyclic_buffer(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_buffer(const std::size_t & _capacity) :
		capacity{ _capacity },
		size{ _capacity , 0 },
		data{ (value_type*)malloc(_capacity * sizeof(value_type)) },
		last_point{ data + _capacity - 1 }
	{
		assert(_capacity > (std::size_t)1);

		write_point = read_point = data;
	}

	~cyclic_buffer()
	{
		if (!size.is_terminated())
			size.terminate();

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

	inline value_type * begin() const
	{
		return data;
	}

	inline value_type * end() const
	{
		return data + capacity;
	}

	inline void push(const value_type & value)
	{
		this->wait_for_space();

		*write_point = value;
		(write_point == last_point ? write_point = data : ++write_point);

		size.add();
	}

	inline value_type pop()
	{
		this->wait_for_data();

		value_type result{ *read_point };
		(read_point == last_point ? read_point = data : ++read_point);

		size.sub();

		return result;
	}

	inline const value_type operator[](const std::size_t & _index) const
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline value_type & operator[](const std::size_t & _index)
	{
		assert(_index < get_size());

		return *(last_point - read_point < _index ? write_point - (get_size() - _index) : read_point + _index);
	}

	inline void wait_for_space() const
	{
		if (size.get_value() == capacity)
			size.wait_for_add();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_space_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (size.get_value() == capacity)
			return size.wait_for_add_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_space_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (size.get_value() == capacity)
			return size.wait_for_add_until(timeout_time);

		return true;
	}

	inline void wait_for_data() const
	{
		if (size.get_value() == 0)
			size.wait_for_sub();
	}

	template<class _Rep, class _Period>
	inline bool wait_for_data_for(const std::chrono::duration<_Rep, _Period>& rel_time) const
	{
		if (size.get_value() == 0)
			return size.wait_for_sub_for(rel_time);

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_for_data_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time) const
	{
		if (size.get_value() == 0)
			return size.wait_for_sub_until(timeout_time);

		return true;
	}

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size.get_value();
	}
};

template<typename _Ty>
class cyclic_buffer_unsafe
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer_unsafe<_Ty> type;

protected:
	value_type * const data;
	value_type * const last_point;
	value_type * front_point;
	value_type * back_point;

	std::size_t size;
	const std::size_t capacity;

public:
	cyclic_buffer_unsafe(type const &) = delete;
	type & operator=(type const &) = delete;

	cyclic_buffer_unsafe(const std::size_t _capacity) :
		capacity{ _capacity },
		data((value_type*)malloc(_capacity * sizeof(value_type))),
		last_point(data + _capacity - 1)
	{
		assert(_capacity > (std::size_t)1);

		front_point = back_point = data;
		size = 0;
	}

	~cyclic_buffer_unsafe()
	{
		free(data);
	}

	inline value_type * begin() const
	{
		return data;
	}

	inline value_type * end() const
	{
		return data + capacity;
	}

	inline value_type push_front(value_type const & _value)
	{
		assert(size < capacity);

		(front_point == data ? front_point = last_point : --front_point);
		++size;

		value_type result{ *front_point };
		*front_point = _value;

		return result;
	}

	inline value_type force_push_front(value_type const & _value)
	{
		(front_point == data ? front_point = last_point : --front_point);
		if (size == capacity)
			back_point = front_point;
		else
			++size;

		value_type result{ *front_point };
		*front_point = _value;

		return result;
	}

	inline value_type push_back(value_type const & _value)
	{
		assert(size < capacity);

		value_type result{ *back_point };
		*back_point = _value;

		(back_point == last_point ? back_point = data : ++back_point);
		++size;

		return result;
	}

	inline value_type force_push_back(value_type const & _value)
	{
		value_type result{ *back_point };
		*back_point = _value;

		(back_point == last_point ? back_point = data : ++back_point);
		if (size == capacity)
			front_point = back_point;
		else
			++size;

		return result;
	}

	inline value_type pop_front()
	{
		assert(size > (std::size_t)0);

		value_type result{ *front_point };

		(front_point == last_point ? front_point = data : ++front_point);
		--size;

		return result;
	}

	inline value_type pop_front(value_type const & _value)
	{
		assert(size > (std::size_t)0);

		value_type result{ *front_point };
		*front_point = _value;

		(front_point == last_point ? front_point = data : ++front_point);
		--size;

		return result;
	}

	inline value_type pop_back()
	{
		assert(size > (std::size_t)0);

		(back_point == data ? back_point = last_point : --back_point);
		--size;

		return (*back_point);
	}

	inline value_type pop_back(value_type const & _value)
	{
		assert(size > (std::size_t)0);

		(back_point == data ? back_point = last_point : --back_point);
		--size;

		value_type result{ *back_point };
		*back_point = _value;

		return result;
	}

	inline value_type operator[](const std::size_t & _index) const
	{
		assert(_index < size);

		if ((last_point < front_point) || ((std::size_t)(last_point - front_point) < _index))
			return *(back_point - (size - _index));

		return *(front_point + _index);
	}

	inline value_type & operator[](const std::size_t & _index)
	{
		assert(_index < size);

		if ((last_point < front_point) || ((std::size_t)(last_point - front_point) < _index))
			return *(back_point - (size - _index));

		return *(front_point + _index);
	}

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size;
	}
};

#endif // !_CYCLIC_BUFFER_H_
