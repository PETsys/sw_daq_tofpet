
OBJS := FrameServer.cpp.o UDPFrameServer.cpp.o Client.cpp.o

CPPFLAGS := $(CPPFLAGS)
CXXFLAGS := -g -O2
LDFLAGS := $(LDFLAGS)  -lpthread -lrt

all: daqd 
daqd: daqd.cpp.o $(OBJS)
	$(CXX) -o $@ daqd.cpp.o $(OBJS) $(LDFLAGS)


%.cpp.o: %.cpp
	$(CXX) -c -o $@ $< $(CXXFLAGS) $(CPPFLAGS)


clean: 
	rm -f daqd *.cpp.o