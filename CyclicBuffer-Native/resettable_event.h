#ifndef _RESETTABLE_EVENT_H_
#define _RESETTABLE_EVENT_H_

#include <time.h>
#include <Windows.h>

class resettable_event
{
protected:
	CONDITION_VARIABLE cv;
	CRITICAL_SECTION sync;
	LONG volatile state;

public:
	resettable_event(const bool initial_state) 
	{
		InitializeConditionVariable(&cv);
		InitializeCriticalSectionAndSpinCount(&sync, INFINITE);

		if (initial_state)
		{
			state = INFINITE;
		}
		else
		{
			state = 0;
		}
	}

	~resettable_event()
	{
		DeleteCriticalSection(&sync);
	}

	inline bool is_set() const
	{
		return (state != 0);
	}

	inline void set()
	{
		EnterCriticalSection(&sync);

		if (InterlockedExchange(&state, INFINITE) == 0)
		{
			WakeAllConditionVariable(&cv);
		}

		LeaveCriticalSection(&sync);
	}

	inline void reset()
	{
		EnterCriticalSection(&sync);

		state = 0;

		LeaveCriticalSection(&sync);
	}

	virtual inline void wait() = 0;

	inline bool wait_for(const DWORD& milliseconds)
	{
		return this->wait_until(clock() + milliseconds);
	}

	virtual inline bool wait_until(const clock_t& /*timeout_time*/) = 0;
};

class manual_reset_event : public resettable_event
{
public:
	explicit manual_reset_event(const bool initial_state = false) : resettable_event(initial_state) { }

	inline void wait()
	{
		EnterCriticalSection(&sync);

		while (state == 0)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		LeaveCriticalSection(&sync);
	}

	inline bool wait_until(const clock_t& timeout_time)
	{
		EnterCriticalSection(&sync);

		while (state == 0)
		{
			if (SleepConditionVariableCS(&cv, &sync, timeout_time - clock()) == 0)
			{
				LeaveCriticalSection(&sync);
				return (state != 0);
			}
		}

		LeaveCriticalSection(&sync);
		return true;
	}
};

class auto_reset_event : public resettable_event
{
public:
	explicit auto_reset_event(const bool initial_state = false) : resettable_event(initial_state) { }

	inline void wait()
	{
		EnterCriticalSection(&sync);

		while (InterlockedExchange(&state, 0) == 0)
		{
			SleepConditionVariableCS(&cv, &sync, INFINITE);
		}

		LeaveCriticalSection(&sync);
	}

	inline bool wait_until(const clock_t& timeout_time)
	{
		EnterCriticalSection(&sync);

		while (InterlockedExchange(&state, 0) == 0)
		{
			if (SleepConditionVariableCS(&cv, &sync, timeout_time - clock()) == 0)
			{
				LeaveCriticalSection(&sync);
				return (InterlockedExchange(&state, 0) != 0);
			}
		}

		LeaveCriticalSection(&sync);
		return true;
	}
};

#endif // !_RESETTABLE_EVENT_H_
