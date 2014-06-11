#ifndef __nDAQ__EVENTSOURCESINK_HPP__DEFINED__
#define __nDAQ__EVENTSOURCESINK_HPP__DEFINED__
#include <stdio.h>
#include "EventBuffer.hpp"

namespace DAQ { namespace Core {

	template <class TEventInput>
	class EventSink {
	public:
		virtual void pushT0(double t0) = 0;
		virtual void pushEvents(EventBuffer<TEventInput> *buffer) = 0;
		virtual void finish() = 0;
		virtual void report() = 0;
		virtual ~EventSink() {};
	};

	template <class TEventOutput>
	class EventSource {
	public:
		EventSource(EventSink<TEventOutput> *sink) {
			this->sink = sink;
		};
		
		virtual ~EventSource() {
			delete this->sink;
		};
	protected:
		EventSink<TEventOutput> *sink;

	};
	
	template <class TEventInput> 
	class NullSink : public EventSink<TEventInput> {
	public:
		NullSink() { count = 0; };
		virtual void pushT0(double t0) {};
		virtual void pushEvents(EventBuffer<TEventInput> *buffer) {
			count += buffer->getSize();
			delete buffer;
		};
		
		virtual void finish() {
		};
		
		virtual void report() {
			fprintf(stderr, ">> NullSink report:\n total events received: %ld\n", count);
		};
		~NullSink() {};
	private:
		long count;
	};
	
}}
#endif
