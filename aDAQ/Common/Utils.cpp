#include "Utils.hpp"
#include <stdio.h>
#include <stdlib.h>

#include <boost/filesystem/path.hpp>
#include <boost/filesystem/operations.hpp>
using namespace boost::filesystem;

const char * DAQ::Common::getCrystalMapFileName()
{
	
	char *name = getenv("ADAQ_CRYSTAL_MAP");
	if(name == NULL) {
		fprintf(stderr, "Error: ADAQ_CRYSTAL_MAP environment variable is not set\n");
		exit(1);
	}
	
	path p(name);
	if(!is_regular_file(p)) {
		fprintf(stderr, "Error: %s does not exist or is not a file\n", name);
		exit(1);
	}
	
	return (const char *) name;
}
