#ifndef __DTFLYP_HPP__DEFINED__
#define __DTFLYP_HPP__DEFINED__

#include <stdint.h>
#include <queue>
#include <pthread.h>
#include <boost/crc.hpp>

namespace DTFLY {
typedef unsigned long DWORD;
typedef unsigned long long UINT64;

extern "C" {
#include "dtfly_p_etpu_defs.h"
}

}

typedef boost::crc_optimal<32, 0x04C11DB7, 0x0A1CB27F, 0, false, false> crcResult_t;

class DtFlyP {
public:
	  DtFlyP();
	  ~DtFlyP();
	  int getWords(uint64_t *buffer, int count);
	  void stopWorker();
	  void startWorker();
	bool cardOK();
	int sendCommand(int febID, char *buffer, int bufferSize, int commandLength);
	int setAcquistionOnOff(bool enable);
	  static const int ETIMEOUT = -1;
	  static const int ENOWORDS = -2;
	  static const int ENOCARD = -10000;

private:
	int getWords_(uint64_t *buffer, int count);
	int WriteAndCheck(int reg, uint32_t *data, int count);


	pthread_t worker;
	pthread_mutex_t lock;
	pthread_cond_t condCleanBuffer;
	pthread_cond_t condDirtyBuffer;
	
	DTFLY::SBufferInit dmaBuffer;

	
	uint64_t *wordBuffer;
	volatile int wordBufferUsed;
	volatile int wordBufferStatus;
	
	
	volatile bool die;
	volatile bool hasWorker;
	static void *runWorker(void *arg);

	pthread_mutex_t hwLock;

	//FILE *logFile;

};

#endif
