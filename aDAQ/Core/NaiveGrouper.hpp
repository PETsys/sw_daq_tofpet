#ifndef __DAQ_CORE_NAIVEGROUPER_HPP__DEFINED__
#define __DAQ_CORE_NAIVEGROUPER_HPP__DEFINED__

#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>

namespace DAQ { namespace Core {
	using namespace DAQ::Common;
	
	class NaiveGrouper : public OverlappedEventHandler<Hit, GammaPhoton> {
	public:
		NaiveGrouper(float radius, double timeWindow1, float minEnergy, float maxEnergy, int maxHits,
			EventSink<GammaPhoton> *sink);
		~NaiveGrouper();
		
		virtual void report();
		
	protected:
		virtual EventBuffer<GammaPhoton> * handleEvents(EventBuffer<Hit> *inBuffer);
			
	private:
		float radius2;
		long long timeWindow1;
		float minEnergy;
		float maxEnergy;
		int maxHits;
		u_int32_t nHits[GammaPhoton::maxHits];
		u_int32_t nHitsOverflow;
		/// put nax hits as option and change buildCoincidence and buildGroup
	}; 
	
}}
#endif
