#ifndef _CYCLIC_NUMBER_H_
#define _CYCLIC_NUMBER_H_

#include <type_traits>
#include <assert.h>
#include <utility>

template<typename _Ty, _Ty _Mod>
class cyclic_number
{
	static_assert(std::is_integral<_Ty>::value, "Error: 'cyclic_number' type must be integral.");
	static_assert(!std::is_const<_Ty>::value, "Error: 'cyclic_number' type can not be const.");
	static_assert(!std::is_volatile<_Ty>::value, "Error: 'cyclic_number' type can not be volatile.");
	static_assert(!std::is_reference<_Ty>::value, "Error: 'cyclic_number' type can not be reference.");
	static_assert((_Mod) > (_Ty)1, "Error: 'cyclic_number' modulus must be greater than 1.");

public:
	typedef _Ty value_type;
	typedef cyclic_number<_Ty, _Mod> type;
	static constexpr _Ty modulus = (_Mod);

protected:
	value_type value_;

public:
	explicit cyclic_number(value_type const & _value)
	{
		assert(validate(_value));

		value_ = _value;
	}

	cyclic_number(type const & other) : cyclic_number(std::move(other.value_)) {  }

	cyclic_number() : cyclic_number((value_type)0) {  }

	type & operator=(type const & other)
	{
		assert(validate(other.value_));

		value_ = other.value_;

		return *this;
	}

	inline value_type const value() const
	{
		return value_;
	}

	inline void value(value_type const & _value)
	{
		assert(validate(_value));

		value_ = _value;
	}

	inline bool operator==(type const & other) const
	{
		assert(validate(other.value_));

		return (value_ == other.value_);
	}

	inline bool operator!=(type const & other) const
	{
		assert(validate(other.value_));

		return (value_ != other.value_);
	}

	inline type & operator++()
	{
		++value_;

		if (value_ >= modulus)
		{
			value_ -= modulus;
		}

		return *this;
	}

	inline type operator++(int)
	{
		type result{ *this };
		operator++();

		return result;
	}

	inline type & operator--()
	{
		if (value_ < (value_type)1)
		{
			value_ += modulus;
		}

		--value_;

		return *this;
	}

	inline type operator--(int)
	{
		type result{ *this };
		operator--();

		return result;
	}

	inline type & operator+=(value_type const & number)
	{
		value_ += number;

		while (value_ >= modulus)
		{
			value_ -= modulus;
		}

		while (value_ < (value_type)0)
		{
			value_ += modulus;
		}

		return *this;
	}

	inline type & operator-=(value_type const & number)
	{
		while (value_ < number)
		{
			value_ += modulus;
		}

		value_ -= number;

		while (value_ >= modulus)
		{
			value_ -= modulus;
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
		assert(validate(other.value_));

		if (value_ <= other.value_)
		{
			return (value_type)(other.value_ - value_);
		}

		return (value_type)((modulus + other.value_) - value_);
	}

	inline value_type counter_clockwise_distance(type const & other) const
	{
		assert(validate(other.value_));

		if (other.value_ <= value_)
		{
			return (value_type)(value_ - other.value_);
		}

		return (value_type)((modulus + value_) - other.value_);
	}

	inline value_type minimum_distance(type const & other) const
	{
		assert(validate(other.value_));

		value_type && distance_1{ (other.value_ >= value_) ? (value_type)(other.value_ - value_) : (value_type)(value_ - other.value_) };
		value_type && distance_2{ (value_type)(modulus - distance_1) };

		return (distance_1 <= distance_2 ? distance_1 : distance_2);
	}

	static inline bool validate(value_type const & value)
	{
		return ((value >= (value_type)0) && (value < modulus));
	}

	static inline value_type normalize(value_type const & value)
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
