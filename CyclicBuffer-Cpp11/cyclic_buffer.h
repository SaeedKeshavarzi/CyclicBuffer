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
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, true> type;
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ true };

private:
	value_type *data, *write_point, *read_point, *last_point;
	std::atomic<std::size_t> size;
	const std::size_t capacity;
	spin_lock sync;

	manual_reset_event read_enable;
	bool terminated;

public:
	cyclic_buffer(const type&) = delete;
	type& operator=(const type&) = delete;

	cyclic_buffer(const std::size_t _capacity) : capacity{ _capacity }
	{
		assert(_capacity > 0);

		write_point = read_point = data = (value_type*)malloc((1 + _capacity) * sizeof(value_type));
		last_point = data + _capacity;
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

	inline value_type* begin()
	{
		return data;
	}

	inline value_type* end()
	{
		return data + (1 + capacity);
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

	inline const value_type operator[](std::size_t index) const
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline value_type& operator[](std::size_t index)
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline void wait_for_data()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
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
public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, true, false> type;
	static constexpr bool is_lock_free{ true };
	static constexpr bool is_recyclable{ false };

private:
	value_type *data, *write_point, *last_point;
	std::atomic<value_type*> read_point;
	std::atomic<std::size_t> size;
	const std::size_t capacity;

	manual_reset_event read_enable;
	bool terminated;

public:
	cyclic_buffer(const type&) = delete;
	type& operator=(const type&) = delete;

	cyclic_buffer(const std::size_t _capacity) : capacity{ _capacity }
	{
		assert(_capacity > 0);

		write_point = read_point = data = (value_type*)malloc((1 + _capacity) * sizeof(value_type));
		last_point = data + _capacity;
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

	inline value_type* begin()
	{
		return data;
	}

	inline value_type* end()
	{
		return data + (1 + capacity);
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

	inline const value_type operator[](std::size_t index) const
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline value_type& operator[](std::size_t index)
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline void wait_for_data()
	{
		if (!read_enable.is_set() && !terminated)
		{
			read_enable.wait();
		}
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
public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, true> type;
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ true };

private:
	value_type *data, *write_point, *read_point, *last_point;
	const std::size_t capacity;
	counter_lock size;

public:
	cyclic_buffer(const type&) = delete;
	type& operator=(const type&) = delete;

	cyclic_buffer(const std::size_t _capacity) : capacity{ _capacity }, size{ _capacity , 0 }
	{
		assert(_capacity > 0);

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

	inline value_type* begin()
	{
		return data;
	}

	inline value_type* end()
	{
		return data + capacity;
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

	inline const value_type operator[](std::size_t index) const
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline value_type& operator[](std::size_t index)
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
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
public:
	typedef _Ty value_type;
	typedef cyclic_buffer<_Ty, false, false> type;
	static constexpr bool is_lock_free{ false };
	static constexpr bool is_recyclable{ false };

private:
	value_type *data, *write_point, *read_point, *last_point;
	const std::size_t capacity;
	counter_lock size;

public:
	cyclic_buffer(const type&) = delete;
	type& operator=(const type&) = delete;

	cyclic_buffer(const std::size_t _capacity) : capacity{ _capacity }, size{ _capacity , 0 }
	{
		assert(_capacity > 0);

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

	inline value_type* begin()
	{
		return data;
	}

	inline value_type* end()
	{
		return data + capacity;
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

	inline const value_type operator[](std::size_t index) const
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
	}

	inline value_type& operator[](std::size_t index)
	{
		assert(index < size);

		return *(last_point - read_point < index ? write_point - (size - index) : read_point + index);
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

	inline std::size_t get_capacity() const
	{
		return capacity;
	}

	inline std::size_t get_size() const
	{
		return size.get_value();
	}
};

template<typename _Ty, std::size_t _Capacity>
class cyclic_buffer_unsafe
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_buffer_unsafe' type can not be reference.");
	static_assert((_Capacity) > (std::size_t)1, "Error: 'cyclic_buffer_unsafe' capacity must be greater than 1.");

public:
	typedef _Ty value_type;
	typedef cyclic_buffer_unsafe<_Ty, _Capacity> type;
	static constexpr std::size_t capacity = (_Capacity);

protected:
	value_type *const data_, *const last_point_, *read_point_, *write_point_;
	std::size_t size_;

public:
	cyclic_buffer_unsafe(type const &) = delete;
	type & operator=(type const &) = delete;

	cyclic_buffer_unsafe() :
		data_((value_type*)malloc(capacity * sizeof(value_type))),
		last_point_(data_ + capacity - 1)
	{
		write_point_ = read_point_ = data_;
		size_ = 0;
	}

	~cyclic_buffer_unsafe()
	{
		free(data_);
	}

	inline value_type * begin() const
	{
		return data_;
	}

	inline value_type * end() const
	{
		return data_ + capacity;
	}

	inline value_type push(value_type const & _value)
	{
		assert(size_ < capacity);

		value_type result = *write_point_;
		*write_point_ = _value;

		(write_point_ == last_point_ ? write_point_ = data_ : ++write_point_);
		++size_;

		return result;
	}

	inline value_type force_push(value_type const & _value)
	{
		value_type result = *write_point_;
		*write_point_ = _value;

		(write_point_ == last_point_ ? write_point_ = data_ : ++write_point_);
		if (size_ == capacity)
		{
			read_point_ = write_point_;
		}
		else
		{
			++size_;
		}

		return result;
	}

	inline value_type pop()
	{
		assert(size_ > (std::size_t)0);

		value_type result = *read_point_;

		(read_point_ == last_point_ ? read_point_ = data_ : ++read_point_);
		--size;

		return result;
	}

	inline value_type pop(value_type const & _value)
	{
		assert(size_ > (std::size_t)0);

		value_type result = *read_point_;
		*read_point_ = _value;

		(read_point_ == last_point_ ? read_point_ = data_ : ++read_point_);
		--size;

		return result;
	}

	inline value_type operator[](std::size_t const & _index) const
	{
		assert(_index < size_);

		return *(last_point_ - read_point_ < _index ? write_point_ - (size_ - _index) : read_point_ + _index);
	}

	inline value_type & operator[](std::size_t const & _index)
	{
		assert(_index < size_);

		return *(last_point_ - read_point_ < _index ? write_point_ - (size_ - _index) : read_point_ + _index);
	}

	inline std::size_t size() const
	{
		return size_;
	}
};

#endif // !_CYCLIC_BUFFER_H_
