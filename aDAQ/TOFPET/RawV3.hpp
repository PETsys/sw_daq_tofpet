#ifndef __TOFPET__RAWV3_HPP__DEFINED__
#define __TOFPET__RAWV3_HPP__DEFINED__
#include <Common/Task.hpp>
#include <TOFPET/Raw.hpp>
#include <Core/EventSourceSink.hpp>
#include <Core/Event.hpp>
#include <Core/RawPulseWriter.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>

namespace DAQ { namespace TOFPET { 
	using namespace ::DAQ::Common;
	using namespace ::DAQ::Core;
	using namespace std;

		typedef __uint128_t RawEventV3;
	/*
	 * frameID	: 36 bit
	 * asicID	: 14 bit
	 * channelID	: 6 bit
	 * tacID	: 2 bit
	 * tCoarse	: 10 bit
	 * eCoarse	: 10 bit
	 * tFine	: 10 bit
	 * eFine	: 10 bit
	 * channelIdleTime/8192 : 15 bit
	 * tacIdleTime/8192: 15 bit
	 */
	
	
	class RawWriterV3 : public RawWriter {
	public:
		RawWriterV3(char *fileNamePrefix);
		virtual ~RawWriterV3();
		virtual void openStep(float step1, float step2);
		virtual void closeStep();
		virtual void addEvent(RawPulse &p);
	private:
		FILE *outputDataFile;
		FILE *outputIndexFile;
		float step1;
		float step2;
		long stepBegin;
	};

	
	class RawScannerV3 : public RawScanner{
		public:
		RawScannerV3(char *indexFilePrefix);
		~RawScannerV3();
		
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
	
	class RawReaderV3 : public RawReader, public EventSource<RawPulse> {
	
	public:
		RawReaderV3(char *dataFilePrefix, float T, unsigned long long eventsBegin, unsigned long long eventsEnd, EventSink<RawPulse> *sink);
		~RawReaderV3();
		
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
