#ifndef _THREAD_NAMING_H_
#define _THREAD_NAMING_H_

#include <thread>

extern void set_thread_name(std::thread& thread, const char* thread_name);
extern void set_current_thread_name(const char* thread_name);

#endif // !_THREAD_NAMING_H_
