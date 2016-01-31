#ifndef __DAQ_ENDOTOFPET__EXTRACT_HPP__DEFINED__
#define __DAQ_ENDOTOFPET__EXTRACT_HPP__DEFINED__
#include <vector>
#include <Core/Event.hpp>
#include <Core/OverlappedEventHandler.hpp>
#include <Common/Instrumentation.hpp>
#include <TOFPET/P2Extract.hpp>
#include <STICv3/sticv3Handler.hpp>
#include <DSIPM/dsipmHandler.hpp>

namespace DAQ { namespace ENDOTOFPET {
		using namespace std;
		using namespace DAQ::Common;
		using namespace DAQ::Core;
		using namespace DAQ::TOFPET;
		using namespace DAQ::STICv3;
		using namespace DAQ::DSIPM;
	
		class Extract : public OverlappedEventHandler<RawHit, Hit> {
		public:
			Extract(DAQ::TOFPET::P2Extract *tofpetH, DAQ::STICv3::Sticv3Handler *sticv3H, DAQ::DSIPM::DsipmHandler *dsipmH,  EventSink<Hit> *sink);
			
			virtual void report();
		
		protected:
			
			
			virtual EventBuffer<Hit> * handleEvents (EventBuffer<RawHit> *inBuffer); 
		   		
			
		private:
			DAQ::TOFPET::P2Extract *tofpetH;
			DAQ::STICv3::Sticv3Handler *sticv3H ;
			DAQ::DSIPM::DsipmHandler *dsipmH;

			volatile u_int32_t nEvent;
			volatile u_int32_t nPassed;
	  	};
	}}
#endif
