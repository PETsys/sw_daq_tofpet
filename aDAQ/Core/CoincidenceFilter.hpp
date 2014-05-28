#ifndef __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__
#define __DAQ__CORE__COINCIDENCEFILTER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
namespace DAQ { namespace Core {

	class CoincidenceFilter : public OverlappedEventHandler<GammaPhoton, GammaPhoton> {
	public:
		CoincidenceFilter(float cWindow, float rWindow, EventSink<GammaPhoton> *sink);
		~CoincidenceFilter();
		virtual void report();
		
	private:
		virtual EventBuffer<GammaPhoton> * handleEvents(EventBuffer<GammaPhoton> *inBuffer);
			
		long long cWindow;
		long long rWindow;
		
		u_int32_t nEventsOut;
		
	};


}}
#endif
