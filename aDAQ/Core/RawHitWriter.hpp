#ifndef __DAQ__CORE__RawHitWriter_HPP__DEFINED__
#define __DAQ__CORE__RawHitWriter_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <vector>

namespace DAQ { namespace Core {
	
	using namespace std;
	using namespace DAQ::Common;
	
	class AbstractRawHitWriter {
	public:
		virtual ~AbstractRawHitWriter();
		virtual void openStep(float step1, float step2) = 0;
		virtual void closeStep() = 0;
		virtual u_int32_t addEventBuffer(long long tMin, long long tMax, EventBuffer<RawHit> *inBuffer) = 0;
	};
	
	class NullRawHitWriter : public AbstractRawHitWriter {
	public:
		NullRawHitWriter();
		virtual ~NullRawHitWriter();
		virtual void openStep(float step1, float step2);
		virtual void closeStep();
		virtual u_int32_t addEventBuffer(long long tMin, long long tMax, EventBuffer<RawHit> *inBuffer);
	};
		

	class RawHitWriterHandler : public OverlappedEventHandler<RawHit, RawHit> {
	public:
		RawHitWriterHandler (AbstractRawHitWriter *writer, EventSink<RawHit> *sink);
		void report();
	protected:
		virtual EventBuffer<RawHit> * handleEvents (EventBuffer<RawHit> *inBuffer);
	private:
		AbstractRawHitWriter *writer;
		u_int32_t nSingleRead;
	};

}}
#endif 
