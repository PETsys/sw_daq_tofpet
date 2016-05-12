#ifndef __DAQ_STICV3__STICV3HANDLER_HPP__DEFINED__
#define __DAQ_STICV3__STICV3HANDLER_HPP__DEFINED__
#include <Core/Event.hpp> 
#include <Common/Instrumentation.hpp>

//Template for a sticv3handler class

namespace DAQ { namespace STICv3 {
	using namespace std;
	using namespace DAQ::Common;
	using namespace DAQ::Core;
	
	class Sticv3Handler{
	public:
		Sticv3Handler(); // construct handler with whatever variables you may need
		~Sticv3Handler();

		void printReport(); //report at the end of handling events

		bool handleEvent(RawHit &rawHit, Hit &Hit); 

		static int compensateCoarse(unsigned coarse, unsigned long long frameID);


	private:
		u_int32_t nPassed;
		u_int32_t nEvent;
		//whatever variables you may need
		
	};
}}
#endif
