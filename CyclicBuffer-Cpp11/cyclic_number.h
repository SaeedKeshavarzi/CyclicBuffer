#ifndef _CYCLIC_NUMBER_H_
#define _CYCLIC_NUMBER_H_

#include <type_traits>
#include <assert.h>
#include <cstdint>
#include <utility>

template<typename _Ty>
class cyclic_number
{
	static_assert(std::is_integral<_Ty>::value, "Error: 'cyclic_number' type must be integral.");
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_number' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_number' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_number' type can not be reference.");

public:
	typedef _Ty value_type;
	typedef cyclic_number<_Ty> type;

protected:
	value_type value_;
	value_type const modulus_;

public:
	cyclic_number(value_type const & _value, value_type const & _modulus) :
		value_{ _value },
		modulus_{ _modulus }
	{
		assert(_modulus > (value_type)1 /* Error: 'cyclic_number' modulus must be greater than 1. */);
		assert(validate(_value, _modulus));
	}

	type & operator=(type const & other)
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		value_ = other.value_;

		return *this;
	}

	inline value_type const value() const
	{
		return value_;
	}

	inline void value(value_type const & _value)
	{
		assert(validate(_value, modulus_));

		value_ = _value;
	}

	inline bool operator==(type const & other) const
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		return (value_ == other.value_);
	}

	inline bool operator!=(type const & other) const
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		return (value_ != other.value_);
	}

	inline type & operator++()
	{
		++value_;

		if (value_ >= modulus_)
		{
			value_ -= modulus_;
		}

		return *this;
	}

	inline type operator++(int32_t)
	{
		type result{ *this };
		operator++();

		return result;
	}

	inline type & operator--()
	{
		if (value_ < (value_type)1)
		{
			value_ += modulus_;
		}

		--value_;

		return *this;
	}

	inline type operator--(int32_t)
	{
		type result{ *this };
		operator--();

		return result;
	}

	inline type & operator+=(value_type const & number)
	{
		value_ += number;

		while (value_ >= modulus_)
		{
			value_ -= modulus_;
		}

		while (value_ < (value_type)0)
		{
			value_ += modulus_;
		}

		return *this;
	}

	inline type & operator-=(value_type const & number)
	{
		while (value_ < number)
		{
			value_ += modulus_;
		}

		value_ -= number;

		while (value_ >= modulus_)
		{
			value_ -= modulus_;
		}

		return *this;
	}

	inline type operator+(value_type const & number) const
	{
		type result{ *this };
		result += number;

		return result;
	}

	inline type operator-(value_type const & number) const
	{
		type result{ *this };
		result -= number;

		return result;
	}

	inline value_type clockwise_distance(type const & other) const
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		if (value_ <= other.value_)
		{
			return (value_type)(other.value_ - value_);
		}

		return (value_type)((modulus_ + other.value_) - value_);
	}

	inline value_type counter_clockwise_distance(type const & other) const
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		if (other.value_ <= value_)
		{
			return (value_type)(value_ - other.value_);
		}

		return (value_type)((modulus_ + value_) - other.value_);
	}

	inline value_type minimum_distance(type const & other) const
	{
		assert(other.modulus_ == modulus_);
		assert(validate(other.value_, modulus_));

		value_type && distance_1{ (other.value_ >= value_) ? (value_type)(other.value_ - value_) : (value_type)(value_ - other.value_) };
		value_type && distance_2{ (value_type)(modulus_ - distance_1) };

		return (distance_1 <= distance_2 ? distance_1 : distance_2);
	}

	static inline bool validate(value_type const & value, value_type const & modulus)
	{
		return ((value >= (value_type)0) && (value < modulus));
	}

	static inline value_type normalize(value_type const & value, value_type const & modulus)
	{
		value_type result{ value };

		while (result >= modulus)
		{
			result -= modulus;
		}

		while (result < (value_type)0)
		{
			result += modulus;
		}

		return result;
	}
};

#endif // !_CYCLIC_NUMBER_H_
