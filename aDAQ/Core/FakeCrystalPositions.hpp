#ifndef __DAQ__CORE_FAKECRYSTALPOSITIONS_HPP__DEFINED__
#define __DAQ__CORE_FAKECRYSTALPOSITIONS_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <vector>
#include <Common/Instrumentation.hpp>

namespace DAQ { namespace Core {

	using namespace std;
	class FakeCrystalPositions : public OverlappedEventHandler<RawHit, Hit> {
	public: 
		FakeCrystalPositions(	
				EventSink<Hit> *sink);
		
		void report();

	protected:
		virtual EventBuffer<Hit> * handleEvents(EventBuffer<RawHit> *inBuffer);

	};
}}
#endif