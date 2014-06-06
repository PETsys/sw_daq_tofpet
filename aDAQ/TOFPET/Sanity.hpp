#ifndef __DAQ_TOFPET__SANITY_HPP__DEFINED__
#define __DAQ_TOFPET__SANITY_HPP__DEFINED__

#include <vector>
#include <Core/Event.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <Common/Instrumentation.hpp>
#include <TOFPET/P2.hpp>

namespace DAQ { namespace TOFPET {
	using namespace std;
	using namespace DAQ::Common;
	using namespace DAQ::Core;
	
	class Sanity : public OverlappedEventHandler<RawPulse, RawPulse> {
	public:
		Sanity(float deadTime, EventSink<RawPulse> *sink);
		
		virtual void report();
		
	protected:
		virtual EventBuffer<RawPulse> * handleEvents (EventBuffer<RawPulse> *inBuffer); 
		
	private:
		long long deadtimeWindow;
		volatile u_int32_t nEvent;
		volatile u_int32_t nOverlapped;
		volatile u_int32_t nDeadtime;
		volatile u_int32_t nPassed;
	};
}}
#endif
