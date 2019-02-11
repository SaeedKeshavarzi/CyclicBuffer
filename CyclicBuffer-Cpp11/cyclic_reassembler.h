#ifndef _CYCLIC_REASSEMBLER_H_
#define _CYCLIC_REASSEMBLER_H_

#include <type_traits>
#include <stddef.h>
#include <assert.h>

#include "cyclic_number.h"

template<typename _Ty, std::size_t _Mod, std::size_t _Size = (_Mod)>
class cyclic_reassembler
{
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_reassembler' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_reassembler' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_reassembler' type can not be reference.");
	static_assert((_Mod) > (std::size_t)1, "Error: 'cyclic_reassembler' modulus must be greater than 1.");
	static_assert((_Size) > (std::size_t)1, "Error: 'cyclic_reassembler' size must be greater than 1.");
	static_assert((_Mod) >= (_Size), "Error: 'cyclic_reassembler' modulus must be greater than or equla to its size.");

public:
	typedef _Ty value_type;
	typedef cyclic_reassembler<_Ty, _Mod, _Size> type;
	static constexpr std::size_t modulus = (_Mod);
	static constexpr std::size_t size = (_Size);

protected:
	typedef cyclic_number<std::size_t, size> local_index_t;
	typedef cyclic_number<std::size_t, modulus> global_index_t;

	local_index_t read_point_{ 0 };
	global_index_t offset_{ 0 };

	value_type *const data_;
	bool *const exist_;

public:
	cyclic_reassembler(const type &) = delete;
	type & operator=(const type &) = delete;

	cyclic_reassembler() :
		data_((value_type*)malloc(size * sizeof(value_type))),
		exist_((bool*)malloc(size * sizeof(bool)))
	{
		memset(exist_, 0, size * sizeof(bool));
	}

	explicit cyclic_reassembler(std::size_t const & _offset) : cyclic_reassembler()
	{
		offset(_offset);
	}

	virtual ~cyclic_reassembler()
	{
		free(data_);
		free(exist_);
	}

	inline value_type * begin() const
	{
		return data_;
	}

	inline value_type * end() const
	{
		return data_ + size;
	}

	inline std::size_t offset() const
	{
		return offset_.value();
	}

	inline void offset(std::size_t const & _offset)
	{
		assert(global_index_t::validate(_offset));

		for (/* nothing */; offset_.value() != _offset; ++offset_)
		{
			exist_[read_point_.value()] = false;
			++read_point_;
		}
	}

	inline bool valid_index(std::size_t const & _index) const
	{
		assert(global_index_t::validate(_index));

		return (offset_.clockwise_distance((global_index_t)_index) < size);
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
		memset(exist_, 0, size * sizeof(bool));
	}

	inline std::size_t ready_count() const
	{
		std::size_t result{ 0 };
		local_index_t it{ read_point_ };
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
		local_index_t local_index = local_index_(_index);

		value_type result = data_[local_index.value()];
		data_[local_index.value()] = _value;
		exist_[local_index.value()] = true;

		return result;
	}

	inline value_type force_push(value_type const & _value, std::size_t const & _index)
	{
		if (!valid_index(_index))
		{
			std::size_t && diff = offset_.clockwise_distance((global_index_t)_index) - (size - (std::size_t)1);
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

		return result;
	}

protected:
	inline local_index_t local_index_(std::size_t const & _index) const
	{
		assert(global_index_t::validate(_index));

		std::size_t && diff = offset_.clockwise_distance((global_index_t)_index);
		assert(diff < size);

		return (read_point_ + diff);
	}
};

#endif // !_CYCLIC_REASSEMBLER_H_
