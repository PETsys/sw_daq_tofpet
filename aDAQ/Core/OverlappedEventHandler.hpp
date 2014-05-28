#ifndef __DAQ_CORE_OVERLAPPEDEVENTHANDLER_HPP__DEFINED__
#define __DAQ_CORE_OVERLAPPEDEVENTHANDLER_HPP__DEFINED__
#include <Core/EventSourceSink.hpp>
#include <Core/EventBuffer.hpp>
#include <pthread.h>
#include <deque>
#include <boost/timer.hpp>
#include <Core/ThreadPool.hpp>

namespace DAQ { namespace Core {

	template <class TEventInput, class TEventOutput>
	class OverlappedEventHandler : 
		public EventSink<TEventInput>,
		public EventSource<TEventOutput> {
	public:
		static const long overlap = 200000; // 200 ns
	
		OverlappedEventHandler(EventSink<TEventOutput> *sink, bool singleWorker = false, ThreadPool *pool = GlobalThreadPool);
		~OverlappedEventHandler();
		virtual void pushT0(double t0);
		virtual void pushEvents(EventBuffer<TEventInput> *buffer);
		virtual void finish();
		virtual void report();

	protected:
		virtual EventBuffer<TEventOutput> * handleEvents(EventBuffer<TEventInput> *inBuffer) = 0;
		


	private:
		boost::timer dbgTimer;
		bool singleWorker;
		ThreadPool *threadPool;
		
		unsigned peakThreads;
		unsigned nBlocks;
		double blockProcessingTime;

					
		EventBuffer<TEventInput> *lastBuffer;
	
		void extractWorker();

		class Worker {
		public:
			Worker(OverlappedEventHandler<TEventInput, TEventOutput> *master, 
				EventBuffer<TEventInput> *inBuffer);
			~Worker();

		
			OverlappedEventHandler<TEventInput, TEventOutput> *master;
			EventBuffer<TEventInput> *inBuffer;
			EventBuffer<TEventOutput> *outBuffer;
			
			ThreadPool::Job *job;
			
		
			bool isFinished();
			void wait();
			double runTime;
			static void* run(void *);		
		};

		std::deque<Worker *> workers;
	};

#include "OverlappedEventHandler.tpp"

}}
#endif
