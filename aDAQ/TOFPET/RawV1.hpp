#ifndef __TOFPET__RAWV1_HPP__DEFINED__
#define __TOFPET__RAWV1_HPP__DEFINED__
#include <Common/Task.hpp>
#include <Core/EventSourceSink.hpp>
#include <Core/Event.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>

namespace DAQ { namespace TOFPET { 
	using namespace ::DAQ::Common;
	using namespace ::DAQ::Core;
	using namespace std;
	
	struct RawEventV1 {
		uint32_t frameID;
		uint16_t asicID;
		uint16_t channelID;
		uint16_t tacID;
		uint16_t tCoarse;
		uint16_t eCoarse;
		uint16_t xSoC;
		uint16_t tEoC;
		uint16_t eEoC;
	};

	
	class RawScannerV1 {
	public:
		RawScannerV1(FILE *indexFile);
		~RawScannerV1();
		
		int getNSteps();
		void getStep(int stepIndex, float &step1, float &step2, unsigned long &eventsBegin, unsigned long &eventsEnd);
	private:
		struct Step {
			float step1;
			float step2;
			unsigned long eventsBegin;
			unsigned long eventsEnd;
		};
		
		vector<Step> steps;
		
	};
	
	class RawReaderV1 : public ThreadedTask, public EventSource<RawPulse> {
	
	public:
		RawReaderV1(FILE *dataFile, float T, unsigned long eventsBegin, unsigned long eventsEnd, EventSink<RawPulse> *sink);
		~RawReaderV1();
		
		virtual void run();

	private:
		unsigned long eventsBegin;
		unsigned long eventsEnd;
		FILE *dataFile;
		double T;
		bool isOutlier(long long currentFrameID, FILE *dataFile, int entry, int N);
	};
	
}}
#endif