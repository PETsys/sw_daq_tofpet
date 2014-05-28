#define __DAQ_CORE_THREADPOOL_CPP__DEFINED__
#include "ThreadPool.hpp"
#include <unistd.h>
#include <stdio.h>
#include <climits>

using namespace DAQ::Core;
using namespace std;

static ThreadPool _pool(UINT_MAX);
namespace DAQ { namespace Core {
	ThreadPool *GlobalThreadPool = &_pool;
}}

ThreadPool::ThreadPool(unsigned maxWorkers)
	: maxWorkers(maxWorkers), queue(0, (Job *)NULL), workers(0, (Worker *)NULL), nClients(0)
{
	int nCPUs = sysconf(_SC_NPROCESSORS_ONLN);
	if(maxWorkers > nCPUs)
		maxWorkers = nCPUs;
	this->maxWorkers = maxWorkers;
	
	maxQueueSize = 2;
	
	die = true;
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condJobQueued, NULL);
	pthread_cond_init(&condJobStarted, NULL);
}

ThreadPool::~ThreadPool()
{
	die = true;
	pthread_cond_broadcast(&condJobQueued);
	for(unsigned i = 0; i <  workers.size(); i++) {
		delete workers[i];
	}
	pthread_cond_destroy(&condJobStarted);
	pthread_cond_destroy(&condJobQueued);
	pthread_mutex_destroy(&lock);
}

void ThreadPool::clientIncrease()
{
	nClients ++;
	if(nClients > 1) return;
	
	die = false;
	for(int i = 0; i < maxWorkers; i++) {
		workers.push_back(new Worker(this));
	}	
}

void ThreadPool::clientDecrease()
{
	nClients = nClients > 1 ? nClients - 1 : 0;
	if(nClients > 0) return;
	
	die = true;
	pthread_cond_broadcast(&condJobQueued);
	for(unsigned i = 0; i <  workers.size(); i++) {
		delete workers[i];
	}
	workers = vector<Worker *>();
}

bool ThreadPool::isFull()
{
	pthread_mutex_lock(&lock);
	bool r = queue.size() >= maxQueueSize;
	pthread_mutex_unlock(&lock);
	return r;
}

ThreadPool::Job *ThreadPool::queueJob(void *(*start_routine)(void *), void *arg)
{
	Job *job = new Job(start_routine, arg);

	pthread_mutex_lock(&lock);
	while(queue.size() >= maxQueueSize) {
		pthread_cond_wait(&condJobStarted, &lock);	
	}	
	queue.push_back(job);		
	pthread_mutex_unlock(&lock);
	pthread_cond_signal(&condJobQueued);
	return job;
}


ThreadPool::Worker::Worker(ThreadPool *pool)
: pool(pool)
{
	pthread_create(&thread, NULL, run, (void*)this);	
}
	

ThreadPool::Worker::~Worker()
{
	pthread_join(thread, NULL);
}

void *ThreadPool::Worker::run(void *arg)
{
	Worker *w = (Worker *)arg;
	ThreadPool *pool = w->pool;

	
	while(true) {
		enum { IDLE, RUN, DIE } state = IDLE;
		Job *job = NULL;
		pthread_mutex_lock(&pool->lock);
		while (state == IDLE) {
			if(pool->die)
				state = DIE;
			else if(pool->queue.size() > 0) {
				job = pool->queue.front();
				pool->queue.pop_front();
				state = RUN;		
			}
			else {
				pthread_cond_wait(&pool->condJobQueued, &pool->lock);
			}
		}
		pthread_mutex_unlock(&pool->lock);
		if(state == DIE) break;		
	
		job->start_routine(job->arg);
		pthread_cond_signal(&pool->condJobStarted);
		
		pthread_mutex_lock(&job->lock);
		job->finished = true;
		pthread_mutex_unlock(&job->lock);
		pthread_cond_signal(&job->condJobFinished);
	}
	
	return NULL;	
}

ThreadPool::Job::Job(void *(*start_routine)(void *), void *arg)
	: start_routine(start_routine), arg(arg), finished(false)
{
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&condJobFinished, NULL);
}

ThreadPool::Job::~Job()
{
	pthread_cond_destroy(&condJobFinished);
	pthread_mutex_destroy(&lock);
}

bool ThreadPool::Job::isFinished()
{
	bool r = false;
	pthread_mutex_lock(&lock);
	r = finished;
	pthread_mutex_unlock(&lock);
	return r;
}

void ThreadPool::Job::wait()
{
	pthread_mutex_lock(&lock);
	while (!finished)
		pthread_cond_wait(&condJobFinished, &lock);
	pthread_mutex_unlock(&lock);
}
