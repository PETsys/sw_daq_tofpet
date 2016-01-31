#ifndef __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__
#define __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <Common/SystemInformation.hpp>
#include <vector>

namespace DAQ { namespace Core {

	class CoincidenceFilter : public OverlappedEventHandler<RawHit, RawHit> {
	public:
		CoincidenceFilter(DAQ::Common::SystemInformation *systemInformation, float cWindow, float minToT, EventSink<RawHit> *sink);
		~CoincidenceFilter();
		virtual void report();
		
	private:
		virtual EventBuffer<RawHit> * handleEvents(EventBuffer<RawHit> *inBuffer);
			
		long long cWindow;
		long long minToT;
		
		DAQ::Common::SystemInformation *systemInformation;
		
		u_int32_t nEventsIn;
		u_int32_t nTriggersIn;
		u_int32_t nEventsOut;
		
	};


}}
#endif
