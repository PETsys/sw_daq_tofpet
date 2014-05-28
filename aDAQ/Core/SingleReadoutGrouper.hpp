#ifndef __DAQ__CORE__SINGLEREADOUTGROUPER_HPP__DEFINED__
#define __DAQ__CORE__SINGLEREADOUTGROUPER_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <vector>

namespace DAQ { namespace Core {
	
	using namespace std;
	using namespace DAQ::Common;

	class SingleReadoutGrouper : public OverlappedEventHandler<Pulse, RawHit> {		
	public:
		SingleReadoutGrouper (EventSink<RawHit> *sink);
		void report();
	protected:
		virtual EventBuffer<RawHit> * handleEvents (EventBuffer<Pulse> *inBuffer);
	private:
		u_int32_t nSingleRead;
	};

}}
#endif 
