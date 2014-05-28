#ifndef __DAQ__CORE__COINCIDENCEGROUPER_HPP__DEFINED__
#define __DAQ__CORE__COINCIDENCEGROUPER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
namespace DAQ { namespace Core {

	class CoincidenceGrouper : public OverlappedEventHandler<GammaPhoton, Coincidence> {
	public:
		CoincidenceGrouper(float cWindow, EventSink<Coincidence> *sink);
		~CoincidenceGrouper();
		virtual void report();
		
	private:
		virtual EventBuffer<Coincidence> * handleEvents(EventBuffer<GammaPhoton> *inBuffer);
			
		long long cWindow;
		
		u_int32_t nPrompts;
		
	};


}}
#endif
