#ifndef _SPIN_LOCK_H_
#define _SPIN_LOCK_H_

#include <Windows.h>

class spin_lock
{
private:
	LONG volatile base;

public:
	spin_lock() : base(0) { }

	inline void lock()
	{
		while (InterlockedBitTestAndSet(&base, 0));
	}

	inline void unlock()
	{
		base = 0;
	}
};

#endif // !_SPIN_LOCK_H_
