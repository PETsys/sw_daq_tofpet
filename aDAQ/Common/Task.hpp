#ifndef __DAQ_COMMON_TASK_HPP__DEFINED__
#define __DAQ_COMMON_TASK_HPP__DEFINED__

#include <pthread.h>
#include "Exception.hpp"

namespace DAQ { namespace Common {
	// The base Task class
	class Task {
	public:
		// Call this to tell the task it must stop as soon as possible
		virtual void stop();
		virtual void abort() = 0;
		// Return true if the task is running, false if it's fininshed.
		// May thrown (Exception *), in which case the task will be finished 
		// and the (Exception *) will be reported
		virtual bool poll()  = 0;
		virtual void wait() = 0;

		virtual ~Task() {};
	};

	// A skeleton for tasks that are implemented in other threads.
	// Usage:
	// 1.a Call the TreadedTask() constructor in the new classes' constructors-
	// 1.b Call the start() method in the end of the new classes' constructors.
	// 2.a Overide the run() method to do the real work.
	// 2.b Use lock() and unlock() arround access to members but not methods.
	// 2.c Make run() end when isStopped() is true.
	// 2.d Report errors in run() by throwing an (Exception *).
	
	static void * runTask(void *);
	class ThreadedTask : public Task {
	private:
		pthread_t myThread;
		pthread_mutex_t myMutex;
		pthread_cond_t myCond;
		bool running;
		bool finished;
		Exception * myException;
		void markEnd();
		void markEnd(Exception & e);
	protected:
		virtual void run() = 0;
		void start();
		bool isStopped();
		void lock();
		void unlock();

	public:
		ThreadedTask();
		void abort();
		bool poll();	
		void wait();
		~ThreadedTask();

		friend void * runTask(void *);
	};
	
	
}}
#endif
