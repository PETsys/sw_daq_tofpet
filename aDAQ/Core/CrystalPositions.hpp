#ifndef __DAQ__CORE_CRYSTALPOSITIONS_HPP__DEFINED__
#define __DAQ__CORE_CRYSTALPOSITIONS_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <vector>
#include <Common/Instrumentation.hpp>

namespace DAQ { namespace Core {

	using namespace std;
	class CrystalPositions : public OverlappedEventHandler<RawHit, Hit> {
	public: 
		CrystalPositions(int nCrystals, const char *mapFileName,
				EventSink<Hit> *sink);
				
		virtual ~CrystalPositions();
		
		void report();
		
	private:
		int nCrystals;
		struct Entry {
			int	region;
			int	xi;
			int	yi;
			float	x;
			float	y;
			float	z;
		};
		
		Entry *map;
		u_int32_t nEventsIn;
		u_int32_t nEventsOut;
			

	protected:
		virtual EventBuffer<Hit> * handleEvents(EventBuffer<RawHit> *inBuffer);

	};
}}
#endif