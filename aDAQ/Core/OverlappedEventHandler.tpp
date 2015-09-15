
template <class TEventInput, class TEventOutput>
OverlappedEventHandler<TEventInput, TEventOutput>::OverlappedEventHandler(EventSink<TEventOutput> *sink, bool singleWorker, ThreadPool *pool)
: EventSource<TEventOutput>(sink), singleWorker(singleWorker), threadPool(singleWorker ? new ThreadPool(1) : pool)
{
	threadPool->clientIncrease();
	this->lastBuffer = NULL;

	peakThreads = 0;
	stepProcessingNBlocks = 0;
	stepProcessingTime = 0;
	stepProcessingNInputEvents = 0;
	
}

template <class TEventInput, class TEventOutput>
OverlappedEventHandler<TEventInput, TEventOutput>::~OverlappedEventHandler()
{
	threadPool->clientDecrease();
	if(singleWorker)
		delete threadPool;
}

template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::pushEvents(EventBuffer<TEventInput> *buffer)
{	
	if(buffer == NULL) 
		return;

	stepProcessingNBlocks++;
	peakThreads = workers.size() > peakThreads ? workers.size() : peakThreads;	
	
	
	while(workers.size() > 0 && workers.front()->isFinished()) {
		extractWorker();
	}

	while(workers.size() > 0 && threadPool->isFull()) {
                extractWorker();
        }
	
        while(singleWorker && workers.size() > 0) {
                extractWorker();
        }
	

	if(lastBuffer != NULL) {	
		long long tMax = lastBuffer->getTMax();

		if(overlap > 0) {
			for(unsigned i = 0; i < buffer->getSize(); i++) {
				TEventInput &e = buffer->get(i);
				if(e.time - tMax >= 2*overlap) break;
				lastBuffer->push(e);
			}
		}
		
		
		Worker *worker = new Worker(this, lastBuffer);	
		workers.push_back(worker);
	}
	lastBuffer = buffer;
}

template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::finish()
{
	if(lastBuffer != NULL) {
		Worker *worker = new Worker(this, lastBuffer);	
		workers.push_back(worker);
	}
	lastBuffer = NULL;

	while (workers.size() > 0) {
		extractWorker();
	}
	this->sink->finish();
}

template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::report()
{
	fprintf(stderr, " thread pool\n");
	fprintf(stderr, "   %10.4lf milliseconds/block\n", stepProcessingTime/stepProcessingNBlocks*1000);
	fprintf(stderr, "   %10d peak threads\n", peakThreads);

	this->sink->report();
}


template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::pushT0(double t0)
{
	this->sink->pushT0(t0);
}

template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::extractWorker()
{
	Worker *worker = workers.front();
	workers.pop_front();	
	worker->wait();
	this->sink->pushEvents(worker->outBuffer);
	stepProcessingTime += worker->runTime;	
	stepProcessingNInputEvents += worker->runEvents;
	delete worker;
}

template <class TEventInput, class TEventOutput>
OverlappedEventHandler<TEventInput, TEventOutput>::Worker::Worker(
	OverlappedEventHandler<TEventInput, TEventOutput> *master, 
	EventBuffer<TEventInput> *inBuffer)
{
	this->master = master;
	this->inBuffer = inBuffer;
	this->outBuffer = NULL;
	job = master->threadPool->queueJob(OverlappedEventHandler<TEventInput, TEventOutput>::Worker::run, (void*)this);
}

template <class TEventInput, class TEventOutput>
OverlappedEventHandler<TEventInput, TEventOutput>::Worker::~Worker()
{
}


template <class TEventInput, class TEventOutput>
void *OverlappedEventHandler<TEventInput, TEventOutput>::Worker::run(void *arg)
{
	Worker *w = (Worker *)arg;	
	struct timespec t0;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
	w->runEvents = w->inBuffer->getSize();
	
	w->outBuffer = w->master->handleEvents(w->inBuffer);
	
	struct timespec t1;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
	w->runTime = t1.tv_sec - t0.tv_sec + 1E-9*(t1.tv_nsec - t0.tv_nsec);
	return NULL;
}


template <class TEventInput, class TEventOutput>
bool OverlappedEventHandler<TEventInput, TEventOutput>::Worker::isFinished()
{
	return job->isFinished();
}

template <class TEventInput, class TEventOutput>
void OverlappedEventHandler<TEventInput, TEventOutput>::Worker::wait()
{
	job->wait();
}
