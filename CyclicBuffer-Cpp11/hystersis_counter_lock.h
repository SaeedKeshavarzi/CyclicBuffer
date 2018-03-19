#ifndef _HYSTERSIS_COUNTER_LOCK_H_
#define _HYSTERSIS_COUNTER_LOCK_H_

#include <condition_variable>
#include <atomic>

#include "spin_lock.h"

class hystersis_counter_lock
{
protected:
	const int max_value;
	const int unlock_threshold_down;
	const int unlock_threshold_up;

	std::atomic<int> value;
	bool add_lock, sub_lock;

	std::condition_variable_any cv;
	spin_lock sync;
	bool terminated;

public:
	hystersis_counter_lock(const hystersis_counter_lock&) = delete;
	hystersis_counter_lock& operator=(const hystersis_counter_lock&) = delete;

	explicit hystersis_counter_lock(const int _max_value,
		const int _unlock_threshold_down,
		const int _unlock_threshold_up,
		const int _initial_value = 0) :
		max_value{ _max_value },
		unlock_threshold_down{ _unlock_threshold_down },
		unlock_threshold_up{ _unlock_threshold_up }
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

	inline void add()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (add_lock && !terminated)
		{
			cv.wait(lock);
		}

		const int &&copy = 1 + value.fetch_add(1);

		if (copy == max_value)
		{
			add_lock = true;
		}

		if (sub_lock && (copy >= unlock_threshold_down))
		{
			sub_lock = false;
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

	inline void sub()
	{
		std::unique_lock<spin_lock> lock(sync);

		while (sub_lock && !terminated)
		{
			cv.wait(lock);
		}

		const int &&copy = value.fetch_sub(1) - 1;

		if (copy == 0)
		{
			sub_lock = true;
		}

		if (add_lock && (copy <= max_value - unlock_threshold_up))
		{
			add_lock = false;
			cv.notify_all();
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

	inline int get_value() const
	{
		return value.load();
	}
};

#endif // !_HYSTERSIS_COUNTER_LOCK_H_
