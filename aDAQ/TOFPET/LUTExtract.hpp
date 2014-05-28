#ifndef __DAQ_CORE__LUTEXTRACT_HPP__DEFINED__
#define __DAQ_CORE__LUTEXTRACT_HPP__DEFINED__

#include <vector>
#include <Core/Event.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <Common/Instrumentation.hpp>
#include <TOFPET/LUT.hpp>

namespace DAQ { namespace TOFPET {
	using namespace std;
	using namespace DAQ::Common;
	using namespace DAQ::Core;
	
	class LUTExtract : public OverlappedEventHandler<RawPulse, Pulse> {
	public:
		LUTExtract(DAQ::TOFPET::LUT *lut, bool killZeroToT, EventSink<Pulse> *sink);
		
		virtual void report();
		
	protected:
		virtual EventBuffer<Pulse> * handleEvents (EventBuffer<RawPulse> *inBuffer); 
		
	private:
		DAQ::TOFPET::LUT *lut;
		
		bool killZeroToT;
		volatile u_int32_t nEvent;
		volatile u_int32_t nZeroToT;
		volatile u_int32_t nPassed;
		volatile u_int32_t nNotNormal;
	};
}}
#endif