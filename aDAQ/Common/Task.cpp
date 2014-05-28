#include "Task.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

using namespace DAQ::Common;

void Task::stop()
{
}

static void * ::DAQ::Common::runTask(void * arg)
{
	ThreadedTask * task = (ThreadedTask *)arg;
	try {
		task->run();
		task->markEnd();
	}
	catch (Exception & e) {
		task->markEnd(e);
	}
	catch (Exception * e) {
		fprintf(stderr, "OOPS, threw an Exception * for %s\n", e->getErrorString().c_str());
		task->markEnd(*e);
		delete e;
	}
	catch (std::exception & e)
	{
		fprintf(stderr, "OOPS, threw an std::exception for %s\n", e.what());
	}
	return NULL;
}

ThreadedTask::ThreadedTask()
{
	pthread_mutex_init(&myMutex, NULL);
	pthread_cond_init(&myCond, NULL);
	running = true;
	finished = false;
	myException = NULL;
}

ThreadedTask::~ThreadedTask()
{
	stop();
	pthread_join(myThread, NULL);
	pthread_cond_broadcast(&myCond);
	pthread_cond_destroy(&myCond);
	pthread_mutex_destroy(&myMutex);
	
}

void ThreadedTask::start() {
	pthread_create(&myThread, NULL, runTask, (void*)this);	
}


void ThreadedTask::abort() {
	lock(); {
		running = false;
	} unlock();
	
}

bool ThreadedTask::poll()
{
	Exception * e;
	bool result;

	lock(); {
		e = myException;
		result = !finished;
	} unlock();

	if (e != NULL) {
		ExceptionCarrier ec(*e);
		delete e;
		e = NULL;
		ec.rethrow();
	}
	return result;
}

void ThreadedTask::wait()
{
	pthread_mutex_lock(&myMutex);
	if (!finished) 
		pthread_cond_wait(&myCond, &myMutex);
	pthread_mutex_unlock(&myMutex);
	
	while (poll());
}

void ThreadedTask::markEnd() 
{
	lock(); {
		myException = NULL;
		running = false;
		finished = true;
	} unlock();
	pthread_cond_broadcast(&myCond);
}

void ThreadedTask::markEnd(Exception & e) 
{
	lock(); {
		myException = e.clone();
		running = false;
		finished = true;
	} unlock();
	pthread_cond_broadcast(&myCond);
}


bool ThreadedTask::isStopped()
{
	bool result;
	lock(); {
		result = !running;
	} unlock();
	return result;
}

void ThreadedTask::lock()
{
	pthread_mutex_lock(&myMutex);
}

void ThreadedTask::unlock()
{
	pthread_mutex_unlock(&myMutex);
}
