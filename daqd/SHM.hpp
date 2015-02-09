#include "Protocol.hpp"
#include <sys/types.h>
#include <string>

class SHM {
public:
  SHM(std::string path);
  ~SHM();

  unsigned long long getSize();
  unsigned long long getFrameID(int index);
  bool getFrameLost(int index);
  int getNEvents(int index);
  int getEventType(int index, int event);
  int getTCoarse(int index, int event);
  int getECoarse(int index, int event);
  int getTFine(int index, int event);
  int getEFine(int index, int event);
  int getAsicID(int index, int event);
  int getChannelID(int index, int event);
  int getTACID(int index, int event);
  long long getTACIdleTime(int index, int event);
  long long getChannelIdleTime(int index, int event);
  

private:
  int shmfd;
  DataFrame *shm;
  off_t shmSize;
  
};
