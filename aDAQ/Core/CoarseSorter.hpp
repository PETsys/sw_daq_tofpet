#ifndef __DAQ__CORE__CoarseSorter_HPP__DEFINED__
#define __DAQ__CORE__CoarseSorter_HPP__DEFINED__
#include "Event.hpp"
#include "OverlappedEventHandler.hpp"
#include <Common/Instrumentation.hpp>
#include <vector>

namespace DAQ { namespace Core {
	
	using namespace std;
	using namespace DAQ::Common;

	/*! Sorts RawPulse events into chronological order by their (coarse) time tag.
	 * Events are correctly sorted in relation to their frame ID, while ithing a frame, 
	 * they are sorted with a tolerance of overalp/2.
	 * Overlap/2 precision is good enough for the remainding of the software processing chain.
	 * Having correct frame boundaries is convenient for modules which write out events grouped by frame.
	 */
	 
	class CoarseSorter : public OverlappedEventHandler<RawPulse, RawPulse> {
	public:
		CoarseSorter (EventSink<RawPulse> *sink);
		void report();
	protected:
		virtual EventBuffer<RawPulse> * handleEvents (EventBuffer<RawPulse> *inBuffer);
	private:
		u_int32_t nSingleRead;
	};

}}
#endif 
