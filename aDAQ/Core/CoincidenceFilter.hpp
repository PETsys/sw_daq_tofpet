#ifndef __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__
#define __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <Common/SystemInformation.hpp>
#include <vector>

namespace DAQ { namespace Core {

	class CoincidenceFilter : public OverlappedEventHandler<RawPulse, RawPulse> {
	public:
		CoincidenceFilter(DAQ::Common::SystemInformation *systemInformation, float cWindow, float minToT, EventSink<RawPulse> *sink);
		~CoincidenceFilter();
		virtual void report();
		
	private:
		virtual EventBuffer<RawPulse> * handleEvents(EventBuffer<RawPulse> *inBuffer);
			
		long long cWindow;
		long long minToT;
		
		DAQ::Common::SystemInformation *systemInformation;
		
		u_int32_t nEventsIn;
		u_int32_t nTriggersIn;
		u_int32_t nEventsOut;
		
	};


}}
#endif
