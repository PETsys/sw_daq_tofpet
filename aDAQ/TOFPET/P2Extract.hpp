#ifndef __DAQ_CORE__P2EXTRACT_HPP__DEFINED__
#define __DAQ_CORE__P2EXTRACT_HPP__DEFINED__

#include <vector>
#include <Core/Event.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <Common/Instrumentation.hpp>
#include <TOFPET/P2.hpp>

namespace DAQ { namespace TOFPET {
	using namespace std;
	using namespace DAQ::Common;
	using namespace DAQ::Core;
	
	class P2Extract : public OverlappedEventHandler<RawPulse, Pulse> {
	public:
		P2Extract(DAQ::TOFPET::P2 *lut, bool killZeroToT, EventSink<Pulse> *sink);
		
		virtual void report();
		
	protected:
		virtual EventBuffer<Pulse> * handleEvents (EventBuffer<RawPulse> *inBuffer); 
		
	private:
		DAQ::TOFPET::P2 *lut;
		
		bool killZeroToT;
		volatile u_int32_t nEvent;
		volatile u_int32_t nZeroToT;
		volatile u_int32_t nPassed;
		volatile u_int32_t nNotNormal;
	};
}}
#endif