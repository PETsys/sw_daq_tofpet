#ifndef __TOFPET__RAWV2_HPP__DEFINED__
#define __TOFPET__RAWV2_HPP__DEFINED__
#include <Common/Task.hpp>
#include <TOFPET/Raw.hpp>
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
	
	struct RawEventV2 {
		uint32_t frameID;
		uint16_t asicID;
		uint16_t channelID;
		uint16_t tacID;
		uint16_t tCoarse;
		uint16_t eCoarse;
		uint16_t tFine;
		uint16_t eFine;
		int64_t channelIdleTime;
		int64_t tacIdleTime;
	};
	
	class RawWriterV2 : public RawWriter{
	public:
		RawWriterV2(char *fileNamePrefix);
		virtual ~RawWriterV2();
		virtual void openStep(float step1, float step2);
		virtual void closeStep();
		virtual void addEvent(RawHit &p);
	private:
		FILE *outputDataFile;
		FILE *outputIndexFile;
		float step1;
		float step2;
		long stepBegin;
	};

	
	class RawScannerV2 : public RawScanner{
	public:
		RawScannerV2(char *indexFilePrefix);
		~RawScannerV2();
		
		int getNSteps();
		void getStep(int stepIndex, float &step1, float &step2, unsigned long long &eventsBegin, unsigned long long &eventsEnd);
	private:
		struct Step {
			float step1;
			float step2;
			unsigned long long eventsBegin;
			unsigned long long eventsEnd;
		};
		FILE * indexFile;
		vector<Step> steps;
		
	};
	
	class RawReaderV2 : public RawReader, public EventSource<RawHit>{
	
	public:
		RawReaderV2(char *dataFilePrefix, float T, unsigned long long eventsBegin, unsigned long long eventsEnd, EventSink<RawHit> *sink);
		~RawReaderV2();
		
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
