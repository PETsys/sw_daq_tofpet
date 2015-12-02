#ifndef __DAQ__ENDOTOFPET__RAW_HPP__DEFINED__
#define __DAQ__ENDOTOFPET__RAW_HPP__DEFINED__
#include <Common/Task.hpp>
#include <Core/EventSourceSink.hpp>
#include <Core/RawPulseWriter.hpp>
#include <Core/Event.hpp>
#include <TOFPET/Raw.hpp>
#include <stdio.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdint.h>

namespace DAQ { namespace ENDOTOFPET { 
		using namespace ::DAQ::Common;
		using namespace ::DAQ::Core;
		using namespace std;
		
		
		struct StartTime {
			uint8_t code;
			uint64_t time;
		}__attribute__((__packed__));
		
		struct FrameHeader {
			uint8_t code;
			uint64_t frameID;
			uint64_t drift;
		}__attribute__((__packed__));
		
		struct RawTOFPET {
			uint8_t code;
			uint8_t tac;
			uint16_t channelID;
			uint16_t tCoarse;
			uint16_t eCoarse;
			uint16_t tFine;
			uint16_t eFine;
			uint64_t tacIdleTime;
			uint64_t channelIdleTime;
		}__attribute__((__packed__));

		
		struct RawSTICv3 {
			uint8_t code;
			uint16_t channelID;
			uint16_t tCoarse;
			uint16_t eCoarse;
			uint8_t tFine;
			uint8_t eFine;
			bool tBadHit;
			bool eBadHit;
			uint64_t channelIdleTime;
		}__attribute__((__packed__));
		
		

		
		class RawWriterE : public TOFPET::RawWriter {
		public:
			RawWriterE(char *fileNamePrefix, long long acqStartTime);
			virtual ~RawWriterE();
			virtual void openStep(float step1, float step2);
			virtual void closeStep();
			virtual u_int32_t addEventBuffer(long long tMin, long long tMax, EventBuffer<RawPulse> *inBuffer);
			
		private:
			FILE *outputDataFile;
			FILE *outputIndexFile;
			float step1;
			float step2;
			long stepBegin;
			long stepEnd;
			uint64_t currentFrameID;
		};
		
		class RawScannerE : public TOFPET::RawScanner{
		public:
		RawScannerE(char *indexFilePrefix);
		~RawScannerE();
		
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
			
		class RawReaderE : public TOFPET::RawReader, public EventSource<RawPulse> {	  
		public:
			RawReaderE(char *dataFilePrefix, float T, unsigned long long eventsBegin, unsigned long long eventsEnd, EventSink<RawPulse> *sink);

			~RawReaderE();
				  
			virtual void run();
				  
		private:
			long long AcqStartTime;
			long long CurrentFrameID;
			FILE *dataFile;
			double T;
			unsigned long eventsBegin;
			unsigned long eventsEnd;
			
		};
			
	}}
#endif
