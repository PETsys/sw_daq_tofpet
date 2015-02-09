

OBJS := FrameServer.cpp.o DtFlyP.cpp.o DAQFrameServer.cpp.o UDPFrameServer.cpp.o Client.cpp.o 

CPPFLAGS := $(CPPFLAGS) $(shell python-config --includes)
CXXFLAGS := -g -O2  -std=c++0x
LDFLAGS := $(LDFLAGS)  -ldtfly -lwdapi1011 -lpthread -lrt

all: daqd registerTest DSHM.so
headers: Client.hpp FrameServer.hpp UDPFrameServer.hpp DAQFrameServer.hpp DtFlyP.hpp Protocol.hpp SHM.hpp


DSHM.so: SHM.cpp.o DSHM.cpp.o
	$(CXX) -shared -o DSHM.so SHM.cpp.o DSHM.cpp.o $(LDFLAGS) $(shell python-config --ldflags --libs) -lboost_python-mt
	
daqd: daqd.cpp.o $(OBJS)
	$(CXX) -o $@ daqd.cpp.o $(OBJS) $(LDFLAGS)

registerTest: registerTest.cpp.o $(OBJS)
	$(CXX) -o $@ registerTest.cpp.o $(OBJS) $(LDFLAGS)


%.cpp.o: %.cpp $(headers)
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS) -fPIC


clean: 
	rm -f daqd *.cpp.o DSHM.so

.PHONY: all headers clean
