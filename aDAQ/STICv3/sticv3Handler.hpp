#ifndef __DAQ_STICV3__STICV3HANDLER_HPP__DEFINED__
#define __DAQ_STICV3__STICV3HANDLER_HPP__DEFINED__

#include <Core/Event.hpp> // This one will surely be needed...


//Template for a sticv3handler class

namespace DAQ { namespace STICv3 {
	using namespace std;
		//	using namespace DAQ::Common;
		using namespace DAQ::Core;
	
	class Sticv3Handler{
	public:
		Sticv3Handler(); // construct handler with whatever variables you may need
		~Sticv3Handler();

		void printReport(); //report at the end of handling events

		bool handleEvent(RawPulse &rawPulse, Pulse &Pulse); 


	private:
		
		//whatever variables you may need
		
	};
}}
#endif
