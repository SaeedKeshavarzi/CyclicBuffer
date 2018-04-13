#ifndef _COUNTER_LOCK_H_
#define _COUNTER_LOCK_H_

#include <condition_variable>
#include <atomic>

#include "spin_lock.h"

class counter_lock
{
protected:
	const int max_value;

	std::atomic<int> value;
	bool add_lock, sub_lock;

	std::condition_variable_any cv;
	spin_lock sync;
	bool terminated;

public:
	counter_lock(const counter_lock&) = delete;
	counter_lock& operator=(const counter_lock&) = delete;

	counter_lock(const int _max_value, const int _initial_value = 0) : max_value{ _max_value }
	{
		add_lock = (_initial_value == _max_value);
		sub_lock = (_initial_value == 0);
		value = _initial_value;
		terminated = false;
	}

	inline void terminate()
	{
		std::lock_guard<spin_lock> lock(sync);

		terminated = true;
		cv.notify_all();
	}

	inline bool is_terminated() const
	{
		return terminated;
	}

	inline int get_value() const
	{
		return value.load();
	}

	inline void add()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (add_lock && !terminated)
		{
			cv.wait(lock);
		}

		if (value.fetch_add(1) == max_value - 1)
		{
			add_lock = true;
		}

		if (sub_lock)
		{
			sub_lock = false;
			cv.notify_all();
		}
	}

	inline void sub()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (sub_lock && !terminated)
		{
			cv.wait(lock);
		}

		if (value.fetch_sub(1) == 1)
		{
			sub_lock = true;
		}

		if (add_lock)
		{
			add_lock = false;
			cv.notify_all();
		}
	}

	inline void wait_for_add()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (add_lock && !terminated)
		{
			cv.wait(lock);
		}
	}

	inline void wait_for_sub()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (sub_lock && !terminated)
		{
			cv.wait(lock);
		}
	}
};

#endif // !_COUNTER_LOCK_H_
