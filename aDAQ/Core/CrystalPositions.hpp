#ifndef __DAQ__CORE_CRYSTALPOSITIONS_HPP__DEFINED__
#define __DAQ__CORE_CRYSTALPOSITIONS_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <vector>
#include <Common/Instrumentation.hpp>
#include <Common/SystemInformation.hpp>

namespace DAQ { namespace Core {

	using namespace std;
	class CrystalPositions : public OverlappedEventHandler<RawHit, Hit> {
	public: 
		CrystalPositions(DAQ::Common::SystemInformation *systemInformation,
				EventSink<Hit> *sink);
				
		virtual ~CrystalPositions();
		
		void report();
		
	private:
		DAQ::Common::SystemInformation *systemInformation;
		
		uint32_t nEventsIn;
		uint32_t nEventsOut;

	protected:
		virtual EventBuffer<Hit> * handleEvents(EventBuffer<RawHit> *inBuffer);

	};
}}
#endif