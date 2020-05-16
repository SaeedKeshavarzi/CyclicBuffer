#ifndef _SHARED_SPIN_LOCK_H_
#define _SHARED_SPIN_LOCK_H_

#include <atomic>

class shared_spin_lock
{
private:
	std::atomic_int state{ 0 };

public:
	shared_spin_lock() = default;
	shared_spin_lock(const shared_spin_lock&) = delete;
	shared_spin_lock& operator=(const shared_spin_lock&) = delete;

	inline bool try_lock_shared()
	{
		int copy = state.load();

		do
		{
			if (copy == -1)
				return false;
		} while (!state.compare_exchange_strong(copy, copy + 1));

		return true;
	}

	inline void lock_shared()
	{
		int copy = state.load();

		do
		{
			if (copy == -1)
				copy = 0;
		} while (!state.compare_exchange_strong(copy, copy + 1));
	}

	inline void unlock_shared()
	{
		int copy = state.load();

		do
		{
			if (copy <= 0)
				return;
		} while (!state.compare_exchange_strong(copy, copy - 1));
	}

	inline bool try_lock()
	{
		int expected = 0;

		return state.compare_exchange_strong(expected, -1);
	}

	inline void lock()
	{
		int expected = 0;

		while (!state.compare_exchange_strong(expected, -1))
			expected = 0;
	}

	inline void unlock()
	{
		int expected = -1;

		state.compare_exchange_strong(expected, 0);
	}
};

#endif // !_SHARED_SPIN_LOCK_H_
