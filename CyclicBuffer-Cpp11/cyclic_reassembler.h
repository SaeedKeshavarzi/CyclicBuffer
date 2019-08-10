#ifndef _CYCLIC_REASSEMBLER_H_
#define _CYCLIC_REASSEMBLER_H_

#include <condition_variable>
#include <type_traits>
#include <stddef.h>
#include <assert.h>

#include "cyclic_number.h"

template<typename _Ty>
class cyclic_reassembler
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_reassembler' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_reassembler' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_reassembler' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_reassembler<_Ty> type;

protected:
	typedef cyclic_number<std::size_t> index_t;

	const std::size_t modulus_;
	const std::size_t size_;

	index_t read_point_; // base on size_
	index_t offset_; // base on modulus_

	value_type * const data_;
	bool * const exist_;

	bool closing{ false };
	std::condition_variable_any cv;

public:
	cyclic_reassembler(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_reassembler(const std::size_t & _modulus, const std::size_t & _size) :
		modulus_{ _modulus },
		size_{ _size },
		read_point_{ 0, _size },
		offset_{ 0, _modulus },
		data_{ (value_type*)malloc(_size * sizeof(value_type)) },
		exist_{ (bool*)malloc(_size * sizeof(bool)) }
	{
		assert(_modulus > (std::size_t)1 /* Error: 'cyclic_reassembler' modulus must be greater than 1. */);
		assert(_size > (std::size_t)1 /* Error: 'cyclic_reassembler' size must be greater than 1. */);
		assert(_modulus >= _size /* Error: 'cyclic_reassembler' modulus must be greater than or equla to its size. */);

		memset(exist_, 0, _size * sizeof(bool));
	}

	cyclic_reassembler(const std::size_t & _modulus) :
		cyclic_reassembler{ _modulus, _modulus }
	{  }

	virtual ~cyclic_reassembler()
	{
		free(data_);
		free(exist_);

		closing = true;
		cv.notify_all();
	}

	inline value_type * begin() const
	{
		return data_;
	}

	inline value_type * end() const
	{
		return data_ + size_;
	}

	inline std::size_t modulus() const
	{
		return modulus_;
	}

	inline std::size_t size() const
	{
		return size_;
	}

	inline std::size_t offset() const
	{
		return offset_.value();
	}

	inline void offset(const std::size_t & _offset)
	{
		assert(index_t::validate(_offset, modulus_));

		for (/* nothing */; offset_.value() != _offset; ++offset_)
		{
			exist_[read_point_.value()] = false;
			++read_point_;
		}

		cv.notify_all();
	}

	inline bool valid_index(std::size_t const & _index) const
	{
		assert(index_t::validate(_index, modulus_));

		return (offset_.clockwise_distance(index_t(_index, modulus_)) < size_);
	}

	inline bool exist(std::size_t const & _index) const
	{
		if (!valid_index(_index))
		{
			return false;
		}

		return exist_[local_index_(_index).value()];
	}

	inline void clear() const
	{
		memset(exist_, 0, size_ * sizeof(bool));
	}

	inline std::size_t ready_count() const
	{
		std::size_t result{ 0 };
		index_t it{ read_point_ };
		while (exist_[it.value()])
		{
			++result;
			if (++it == read_point_)
			{
				break;
			}
		}

		return result;
	}

	inline value_type push(value_type const & _value, std::size_t const & _index)
	{
		index_t local_index = local_index_(_index);

		value_type result = data_[local_index.value()];
		data_[local_index.value()] = _value;
		exist_[local_index.value()] = true;

		return result;
	}

	template<class Lock>
	inline value_type push(value_type const & _value, std::size_t const & _index, Lock & lock)
	{
		assert(index_t::validate(_index, modulus_));

		index_t wrapped_index{ _index, modulus_ };
		std::size_t diff{ 0 };

		while (((diff = offset_.clockwise_distance(wrapped_index)) >= size_) && !closing) {
			cv.wait(lock);
		}

		std::size_t local_index = (read_point_ + diff).value();
		value_type result = data_[local_index];
		data_[local_index] = _value;
		exist_[local_index] = true;

		return result;
	}

	inline value_type force_push(value_type const & _value, std::size_t const & _index)
	{
		if (!valid_index(_index))
		{
			std::size_t && diff = offset_.clockwise_distance(index_t(_index, modulus_)) - (size_ - (std::size_t)1);
			offset((offset_ + diff).value());
		}

		return push(_value, _index);
	}

	inline value_type pop()
	{
		assert(exist_[read_point_.value()]);

		value_type result = data_[read_point_.value()];
		exist_[read_point_.value()] = false;

		++read_point_;
		++offset_;

		cv.notify_all();

		return result;
	}

	inline value_type pop(const value_type & _value)
	{
		assert(exist_[read_point_.value()]);

		value_type result = data_[read_point_.value()];
		data_[read_point_.value()] = _value;
		exist_[read_point_.value()] = false;

		++read_point_;
		++offset_;

		cv.notify_all();

		return result;
	}

protected:
	inline index_t local_index_(const std::size_t & _index) const
	{
		assert(index_t::validate(_index, modulus_));

		std::size_t && diff = offset_.clockwise_distance(index_t{ _index, modulus_ });
		assert(diff < size_);

		return (read_point_ + diff);
	}
};

#endif // !_CYCLIC_REASSEMBLER_H_
