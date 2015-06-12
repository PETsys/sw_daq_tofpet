#ifndef __DAQ__CORE__RawPulseWriter_HPP__DEFINED__
#define __DAQ__CORE__RawPulseWriter_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <vector>

namespace DAQ { namespace Core {
	
	using namespace std;
	using namespace DAQ::Common;
	
	class AbstractRawPulseWriter {
	public:
		virtual ~AbstractRawPulseWriter();
		virtual void openStep(float step1, float step2) = 0;
		virtual void closeStep() = 0;
		virtual void addEvent(RawPulse &p) = 0;
	};
	
	class NullRawPulseWriter : public AbstractRawPulseWriter {
	public:
		NullRawPulseWriter();
		virtual ~NullRawPulseWriter();
		virtual void openStep(float step1, float step2);
		virtual void closeStep();
		virtual void addEvent(RawPulse &p);
	};
		

	class RawPulseWriterHandler : public OverlappedEventHandler<RawPulse, RawPulse> {
	public:
		RawPulseWriterHandler (AbstractRawPulseWriter *writer, EventSink<RawPulse> *sink);
		void report();
	protected:
		virtual EventBuffer<RawPulse> * handleEvents (EventBuffer<RawPulse> *inBuffer);
	private:
		AbstractRawPulseWriter *writer;
		u_int32_t nSingleRead;
	};

}}
#endif 
