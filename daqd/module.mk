CPPFLAGS := $(CPPFLAGS) $(shell python-config --includes)
CXXFLAGS := -g -O2  -std=c++0x
LDFLAGS := $(LDFLAGS) -lpthread -lrt


HEADERS := Client.hpp FrameServer.hpp UDPFrameServer.hpp DAQFrameServer.hpp DtFlyP.hpp Protocol.hpp SHM.hpp PFP_KX7.hpp
OBJS := FrameServer.cpp.o  UDPFrameServer.cpp.o Client.cpp.o 
ifeq (1, ${DTFLY})
	OBJS := $(OBJS) DtFlyP.cpp.o DAQFrameServer.cpp.o
	CPPFLAGS := $(CPPFLAGS) -D__DTFLY__
	LDFLAGS := $(LDFLAGS)  -ldtfly -lwdapi1011 
endif 

ifeq (1, ${NO_CHANNEL_IDLE_TIME})
	CPPFLAGS := $(CPPFLAGS) -D__NO_CHANNEL_IDLE_TIME__
endif
ifeq (1, ${ENDOTOFPET})
	CPPFLAGS := $(CPPFLAGS) -D__ENDOTOFPET__
endif 
ifeq (1, ${PFP_KX7})
	OBJS := $(OBJS) DAQFrameServer.cpp.o PFP_KX7.cpp.o
	CPPFLAGS := $(CPPFLAGS) -I ./include -DLINUX -D__PFP_KX7__
	LDFLAGS := $(LDFLAGS)  -lpfp_kx7_api -lwdapi1160 
endif 


all: daqd SHM.o DSHM.so


DSHM.so: SHM.cpp.o DSHM.cpp.o
	$(CXX) -shared -o DSHM.so SHM.cpp.o DSHM.cpp.o $(LDFLAGS) $(shell python-config --ldflags --libs) -lboost_python-mt

SHM.o: SHM.cpp.o
	cp SHM.cpp.o SHM.o
	
daqd: daqd.cpp.o $(OBJS)
	$(CXX) -o $@ daqd.cpp.o $(OBJS) $(LDFLAGS)


%.cpp.o: %.cpp $(HEADERS)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS) -fPIC

clean: 
	rm -f daqd *.cpp.o DSHM.so SHM.cpp.o DSHM.cpp.o

.PHONY: all headers clean
