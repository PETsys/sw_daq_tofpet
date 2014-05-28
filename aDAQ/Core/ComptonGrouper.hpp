#ifndef __DAQ_CORE_COMPTONGROUPER_HPP__DEFINED__
#define __DAQ_CORE_COMPTONGROUPER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>

namespace DAQ { namespace Core {
	using namespace DAQ::Common;
	
	class ComptonGrouper : public OverlappedEventHandler<Hit, GammaPhoton> {
	public:
		ComptonGrouper(float radius, double timeWindow, int maxHits, float eMin, float eMax,
			EventSink<GammaPhoton> *sink);
		~ComptonGrouper();
		
		virtual void report();
		
	protected:
		virtual EventBuffer<GammaPhoton> * handleEvents(EventBuffer<Hit> *inBuffer);
			
	private:
		float radius2;
		long long timeWindow;
		int maxHits;
		float eMin;
		float eMax;
		u_int32_t nHits[GammaPhoton::maxHits];
	}; 
	
}}
#endif
