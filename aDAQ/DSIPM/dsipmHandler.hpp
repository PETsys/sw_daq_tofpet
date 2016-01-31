#ifndef __DAQ_DSIPM__DSIPMHANDLER_HPP__DEFINED__
#define __DAQ_DSIPM__DSIPMHANDLER_HPP__DEFINED__

#include <Core/Event.hpp> // This one will surely be needed...


//Template for a sticv3handler class

namespace DAQ { namespace DSIPM {
	using namespace std;
	using namespace DAQ::Core;
	
	class DsipmHandler{
	public:
		DsipmHandler(); // construct handler with whatever variables you may need
		~DsipmHandler();
		void printReport(); //report at the end of handling events

		bool handleEvent(RawHit &raw, Hit &Hit); 		
		  

	private:
		
		//whatever variables you may need
		
	};
}}
#endif
