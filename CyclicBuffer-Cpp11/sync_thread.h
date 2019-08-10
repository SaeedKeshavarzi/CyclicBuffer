#ifndef _SYNC_THREAD_H_
#define _SYNC_THREAD_H_

#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>

class sync_thread_t
{
private:
	mutable std::mutex sync_guard;
	mutable std::shared_mutex config_guard;
	mutable std::condition_variable cv;
	std::atomic_size_t n_threads{ 0 };
	std::atomic_size_t n_involved_thread{ 0 };
	std::atomic_bool terminated{ false };

public:
	sync_thread_t() = default;
	sync_thread_t(const sync_thread_t&) = delete;
	sync_thread_t& operator=(const sync_thread_t&) = delete;

	sync_thread_t(std::size_t _n_threads) :
		n_threads{ _n_threads }
	{

	}

	inline ~sync_thread_t()
	{
		terminate();
	}

	inline bool is_terminated() const
	{
		return terminated;
	}

	inline std::size_t thread_count() const
	{
		return n_threads;
	}

	inline void thread_count(std::size_t count)
	{
		std::lock_guard<std::shared_mutex> lock(config_guard);

		n_threads = count;
	}

	inline void register_thread()
	{
		std::lock_guard<std::shared_mutex> lock(config_guard);

		n_threads.fetch_add(1);
	}

	inline void unregister_thread()
	{
		std::lock_guard<std::shared_mutex> lock(config_guard);

		n_threads.fetch_sub(1);
	}

	inline void reset()
	{
		std::lock_guard<std::shared_mutex> lock(config_guard);

		n_threads = 0;
		n_involved_thread = 0;
		terminated = false;
	}

	inline void terminate()
	{
		std::lock_guard<std::mutex> lock(sync_guard);

		n_involved_thread = 0;
		terminated = true;
		cv.notify_all();
	}

	inline bool sync()
	{
		std::shared_lock<std::shared_mutex> config_lock(config_guard);
		std::unique_lock<std::mutex> lock(sync_guard);

		if (terminated || (n_involved_thread >= n_threads))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
		{
			cv.wait(lock);
		}
		else // (n_involved_thread == n_threads)
		{
			n_involved_thread = 0;
			cv.notify_all();
		}

		return (!terminated);
	}

	template<class _Rep, class _Period>
	inline bool sync_for(const std::chrono::duration<_Rep, _Period>& rel_time)
	{
		std::shared_lock<std::shared_mutex> config_lock(config_guard);
		std::unique_lock<std::mutex> lock(sync_guard);

		if (terminated || (n_involved_thread >= n_threads))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
		{
			if (cv.wait_for(lock, rel_time) == std::cv_status::timeout)
			{
				--n_involved_thread;
				return false;
			}
		}
		else // (n_involved_thread == n_threads)
		{
			n_involved_thread = 0;
			cv.notify_all();
		}

		return (!terminated);
	}

	template<class _Clock, class _Duration>
	inline bool sync_until(const std::chrono::time_point<_Clock, _Duration>& timeout_time)
	{
		std::shared_lock<std::shared_mutex> config_lock(config_guard);
		std::unique_lock<std::mutex> lock(sync_guard);

		if (terminated || (n_involved_thread >= n_threads))
			return false;

		++n_involved_thread;
		if (n_involved_thread < n_threads)
		{
			if (cv.wait_until(lock, timeout_time) == std::cv_status::timeout)
			{
				--n_involved_thread;
				return false;
			}
		}
		else // (n_involved_thread == n_threads)
		{
			n_involved_thread = 0;
			cv.notify_all();
		}

		return (!terminated);
	}
};

#endif // !_SYNC_THREAD_H_
