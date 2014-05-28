#ifndef __DAQ_CORE__COARSEEXTRACT_HPP__DEFINED__
#define __DAQ_CORE__COARSEEXTRACT_HPP__DEFINED__

#include <vector>
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>

namespace DAQ { namespace Core {
	using namespace std;
	using namespace DAQ::Common;
	
	class CoarseExtract : public OverlappedEventHandler<RawPulse, Pulse> {
	public:
		CoarseExtract(bool killZeroToT, EventSink<Pulse> *sink);
		
		virtual void report();
		
	protected:
		virtual EventBuffer<Pulse> * handleEvents (EventBuffer<RawPulse> *inBuffer);
		
	private:	
		bool killZeroToT;
		volatile u_int32_t nEvent;
		volatile u_int32_t nZeroToT;
		volatile u_int32_t nPassed;
	};
}}
#endif
