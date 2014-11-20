
OBJS := FrameServer.cpp.o DtFlyP.cpp.o DAQFrameServer.cpp.o UDPFrameServer.cpp.o Client.cpp.o 

CPPFLAGS := $(CPPFLAGS) -I/home/projects/tofpet/DAQ/PCIe_reg_SW/dtFLY_P_ETPU_installation/linux/include 
CXXFLAGS := -g -O2  -std=c++0x
LDFLAGS := $(LDFLAGS)  -ldtfly -lwdapi1011 -lpthread -lrt

all: daqd registerTest headers
headers: Client.hpp FrameServer.hpp UDPFrameServer.hpp DAQFrameServer.hpp DtFlyP.hpp Protocol.hpp

daqd: daqd.cpp.o $(OBJS)
	$(CXX) -o $@ daqd.cpp.o $(OBJS) $(LDFLAGS)

registerTest: registerTest.cpp.o $(OBJS)
	$(CXX) -o $@ registerTest.cpp.o $(OBJS) $(LDFLAGS)


%.cpp.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS)


clean: 
	rm -f daqd *.cpp.o

.PHONY: all headers clean
