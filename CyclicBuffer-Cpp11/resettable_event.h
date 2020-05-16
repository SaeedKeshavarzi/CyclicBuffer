#ifndef _RESETTABLE_EVENT_H_
#define _RESETTABLE_EVENT_H_

#include <atomic>
#include <condition_variable>

#include "spin_lock.h"

template<bool _AutoReset>
class resettable_event;

typedef resettable_event<true> auto_reset_event;
typedef resettable_event<false> manual_reset_event;

template<>
class resettable_event<true> // Auto Reset Event
{
public:
	typedef resettable_event<true> type;
	static constexpr bool is_auto{ true };

	resettable_event(const resettable_event&) = delete;
	resettable_event& operator=(const resettable_event&) = delete;

	resettable_event(const bool initial_state = false) :
		state{ initial_state }
	{ }

	inline bool is_set() const
	{
		return state.load();
	}

	inline void set()
	{
		std::lock_guard<spin_lock> lock(guard);

		if (!state.exchange(true))
			cv.notify_all();
	}

	inline void reset()
	{
		std::lock_guard<spin_lock> lock(guard);
		state = false;
	}

	inline void wait()
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.exchange(false))
			cv.wait(lock);
	}

	template<class _Rep, class _Period>
	inline bool wait_for(const std::chrono::duration<_Rep, _Period> & rel_time)
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.exchange(false))
		{
			if (cv.wait_for(lock, rel_time) == std::cv_status::timeout)
				return state.exchange(false);
		}

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_until(const std::chrono::time_point<_Clock, _Duration> & timeout_time)
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.exchange(false))
		{
			if (cv.wait_until(lock, timeout_time) == std::cv_status::timeout)
				return state.exchange(false);
		}

		return true;
	}

protected:
	std::atomic_bool state;
	mutable spin_lock guard;
	mutable std::condition_variable_any cv;
};

template<>
class resettable_event<false> // Manual Reset Event
{
public:
	typedef resettable_event<false> type;
	static constexpr bool is_auto{ false };

	resettable_event(const resettable_event&) = delete;
	resettable_event& operator=(const resettable_event&) = delete;

	resettable_event(const bool initial_state = false) :
		state{ initial_state }
	{ }

	inline bool is_set() const
	{
		return state.load();
	}

	inline void set()
	{
		std::lock_guard<spin_lock> lock(guard);

		if (!state.exchange(true))
			cv.notify_all();
	}

	inline void reset()
	{
		std::lock_guard<spin_lock> lock(guard);
		state = false;
	}

	inline void wait() const
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.load())
			cv.wait(lock);
	}

	template<class _Rep, class _Period>
	inline bool wait_for(const std::chrono::duration<_Rep, _Period> & rel_time)  const
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.load())
		{
			if (cv.wait_for(lock, rel_time) == std::cv_status::timeout)
				return state.load();
		}

		return true;
	}

	template<class _Clock, class _Duration>
	inline bool wait_until(const std::chrono::time_point<_Clock, _Duration> & timeout_time)  const
	{
		std::unique_lock<spin_lock> lock(guard);

		while (!state.load())
		{
			if (cv.wait_until(lock, timeout_time) == std::cv_status::timeout)
				return state.load();
		}

		return true;
	}

protected:
	std::atomic_bool state;
	mutable spin_lock guard;
	mutable std::condition_variable_any cv;
};

#endif // !_RESETTABLE_EVENT_H_
