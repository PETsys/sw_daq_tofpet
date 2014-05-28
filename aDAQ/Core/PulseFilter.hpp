#ifndef __DAQ__CORE__PULSEFILTER_HPP__DEFINED__
#define __DAQ__CORE__PULSEFILTER_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <vector>

namespace DAQ { namespace Core {
	
	using namespace std;
	using namespace DAQ::Common;

	class PulseFilter : public OverlappedEventHandler<Pulse, Pulse> {		
	public:
		PulseFilter (float eMin, float eMax, EventSink<Pulse> *sink);
		void report();
	protected:
		virtual EventBuffer<Pulse> * handleEvents (EventBuffer<Pulse> *inBuffer);
	private:
		float eMin;
		float eMax;
		u_int32_t nPassed;
	};

}}
#endif 
