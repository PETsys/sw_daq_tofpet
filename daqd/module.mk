CPPFLAGS := $(CPPFLAGS) $(shell python-config --includes)
CXXFLAGS := -g -O2  -std=c++0x
LDFLAGS := $(LDFLAGS) -lpthread -lrt


OBJS := FrameServer.cpp.o  UDPFrameServer.cpp.o Client.cpp.o 
ifeq (1, ${DTFLY})
	OBJS := $(OBJS) DtFlyP.cpp.o DAQFrameServer.cpp.o
	CPPFLAGS := $(CPPFLAGS) -D__DTFLY__
	LDFLAGS := $(LDFLAGS)  -ldtfly -lwdapi1011 
endif 

ifeq (1, ${ENDOTOFPET})
	CPPFLAGS := $(CPPFLAGS) -D__ENDOTOFPET__
endif 

all: daqd SHM.o DSHM.so
headers: Client.hpp FrameServer.hpp UDPFrameServer.hpp DAQFrameServer.hpp DtFlyP.hpp Protocol.hpp SHM.hpp


DSHM.so: SHM.cpp.o DSHM.cpp.o
	$(CXX) -shared -o DSHM.so SHM.cpp.o DSHM.cpp.o $(LDFLAGS) $(shell python-config --ldflags --libs) -lboost_python-mt

SHM.o: SHM.cpp.o
	cp SHM.cpp.o SHM.o
	
daqd: daqd.cpp.o $(OBJS)
	$(CXX) -o $@ daqd.cpp.o $(OBJS) $(LDFLAGS)


%.cpp.o: %.cpp $(headers)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS) -fPIC

clean: 
	rm -f daqd *.cpp.o DSHM.so SHM.cpp.o DSHM.cpp.o

.PHONY: all headers clean
