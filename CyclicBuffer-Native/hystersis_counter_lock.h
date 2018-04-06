#ifndef _HYSTERSIS_COUNTER_LOCK_H_
#define _HYSTERSIS_COUNTER_LOCK_H_

#include <Windows.h>

class hystersis_counter_lock
{
protected:
	const LONG max_value;
	const LONG unlock_threshold_down;
	const LONG unlock_threshold_up;

	LONG volatile value;
	bool add_lock, sub_lock;

	CONDITION_VARIABLE cv;
	CRITICAL_SECTION sync;
	bool terminated;

public:
	hystersis_counter_lock(const LONG _max_value,
		const LONG _unlock_threshold_down,
		const LONG _unlock_threshold_up,
		const LONG _initial_value = 0) :
		max_value(_max_value),
		unlock_threshold_down(_unlock_threshold_down),
		unlock_threshold_up(_unlock_threshold_up)
	{
		InitializeConditionVariable(&cv);
		InitializeCriticalSectionAndSpinCount(&sync, INFINITE);

		add_lock = (_initial_value == _max_value);
		sub_lock = (_initial_value == 0);
		value = _initial_value;
		terminated = false;
	}

	~hystersis_counter_lock()
	{
		DeleteCriticalSection(&sync);
	}

	inline void terminate()
	{
		EnterCriticalSection(&sync);

		terminated = true;
		WakeAllConditionVariable(&cv);

		LeaveCriticalSection(&sync);
	}

	inline bool is_terminated() const
	{
		return terminated;
	}

	inline void add()
	{
		EnterCriticalSection(&sync);

		while (add_lock && !terminated)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		const LONG &&copy = 1 + InterlockedIncrement(&value);

		if (copy == max_value)
		{
			add_lock = true;
		}

		if (sub_lock && (copy >= unlock_threshold_down))
		{
			sub_lock = false;
			WakeAllConditionVariable(&cv);
		}

		LeaveCriticalSection(&sync);
	}

	inline bool wait_for_add()
	{
		EnterCriticalSection(&sync);

		while (add_lock && !terminated)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		LeaveCriticalSection(&sync);

		return !terminated;
	}

	inline void sub()
	{
		EnterCriticalSection(&sync);

		while (sub_lock && !terminated)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		const LONG &&copy = InterlockedDecrement(&value) - 1;

		if (copy == 0)
		{
			sub_lock = true;
		}

		if (add_lock && (copy <= max_value - unlock_threshold_up))
		{
			add_lock = false;
			WakeAllConditionVariable(&cv);
		}

		LeaveCriticalSection(&sync);
	}

	inline bool wait_for_sub()
	{
		EnterCriticalSection(&sync);

		while (sub_lock && !terminated)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		LeaveCriticalSection(&sync);

		return !terminated;
	}

	inline LONG get_value() const
	{
		return value;
	}
};

#endif // !_HYSTERSIS_COUNTER_LOCK_H_
