#ifndef __DAQ__ENDOTOFPET__RAW_HPP__DEFINED__
#define __DAQ__ENDOTOFPET__RAW_HPP__DEFINED__
#include <Common/Task.hpp>
#include <Core/EventSourceSink.hpp>
#include <Core/Event.hpp>
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
			}__attribute__((__packed__));;
			
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
			}__attribute__((__packed__));;


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
			}__attribute__((__packed__));;


       
	
			class RawReader : public ThreadedTask, public EventSource<RawPulse> {
				  
			public:
				  RawReader(FILE *dataFile, float T, EventSink<RawPulse> *sink);

				  ~RawReader();
				  
				  virtual void run();
				  
			private:
				  long long AcqStartTime;
				  long long CurrentFrameID;
				  FILE *dataFile;
				  double T;
				  
			};
			
	  }}
#endif
