#ifndef __TOFPET__RAW_HPP__DEFINED__
#define __TOFPET__RAW_HPP__DEFINED__
#include <Common/Task.hpp>
#include <Core/EventSourceSink.hpp>
#include <Core/Event.hpp>
#include <Core/RawHitWriter.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>

namespace DAQ { namespace TOFPET { 
	using namespace ::DAQ::Common;
	using namespace ::DAQ::Core;
	using namespace std;

	
	class RawWriter : public AbstractRawHitWriter {
	public:
	
		virtual ~RawWriter();
		virtual void openStep(float step1, float step2)=0;
		virtual void closeStep()=0;
		virtual  u_int32_t addEventBuffer(long long tMin, long long tMax, EventBuffer<RawHit> *inBuffer) = 0;
	};

	
	class RawScanner {
	public:
		~RawScanner();
		virtual int getNSteps()=0;
		virtual void getStep(int stepIndex, float &step1, float &step2, unsigned long long &eventsBegin, unsigned long long &eventsEnd)=0;
	};
	
	class RawReader : public ThreadedTask{
	public:
		
		//RawReader();
		~RawReader();
		//virtual void run()=0;
	
	};
	
}}
#endif
