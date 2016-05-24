SRCS := $(wildcard Common/*cpp) $(wildcard Core/*cpp) $(wildcard TOFPET/*cpp) $(wildcard ENDOTOFPET/*cpp) $(wildcard STICv3/*cpp) $(wildcard DSIPM/*cpp)
BSRCS := $(wildcard Apps/*cpp)


CXXFLAGS ?= -g -O2
LIBS := $(LIBS) -lpthread -lrt
CXXFLAGS := $(CXXFLAGS)  -fPIC
CPPFLAGS := $(CPPFLAGS) -I.

CPPFLAGS := $(CPPFLAGS)

LIBS := $(LIBS) $(GLIBS) -L$(shell root-config --libdir --libs) $(shell root-config --auxlibs) -lMinuit -lboost_filesystem -lboost_regex -lboost_system
CPPFLAGS := $(CPPFLAGS) -I$(shell root-config --incdir --cflags)

CPPFLAGS := $(CPPFLAGS) -I../daqd/

ifeq (1, ${NO_CHANNEL_IDLE_TIME})
	CPPFLAGS := $(CPPFLAGS) -D__NO_CHANNEL_IDLE_TIME__
endif 

ifeq (1, ${ENDOTOFPET})
	CPPFLAGS := $(CPPFLAGS) -D__ENDOTOFPET__
endif 

OBJS := $(SRCS:=.o)
BOBJS := $(BSRCS:=.o)
DEPS := $(SRCS:=.d) $(BSRCS:=.d)

APPS := $(foreach APP,$(BSRCS:.cpp=), $(subst Apps/,,$(APP)))
BINARIES := $(BSRCS:=.b)


default: $(APPS)
	
$(APPS): $(BINARIES)
	
	cp -f Apps/$@.cpp.b $@

ifneq ($(MAKECMDGOALS),clean)
     include $(DEPS)
endif

%.cpp.o: %.cpp.d
	@echo Compiling $<
	$(CXX) -c -o $@ $(CXXFLAGS) $(CPPFLAGS) $(@:.o=) 

%.cpp.d: %.cpp ../daqd/SHM.hpp
	@echo Generating dependencies for $<
	$(CPP) -M -MG -MT $@ -o $@ $(CPPFLAGS) $< 
	
%.cpp.b: %.cpp.o $(OBJS) ../daqd/SHM.o
	@echo Linking $@
	$(CXX) -o $@ $< $(OBJS) ../daqd/SHM.o $(CXXFLAGS) $(CPPFLAGS) $(LIBS)	

	
.SECONDARY: $(DEPS) $(OBJS) $(BOBJS) $(BINARIES)
.PHONY: clean
clean:
	find -type f -name '*.cpp.d' -delete
	find -type f -name '*.cpp.o' -delete
	find -type f -name '*.cpp.b' -delete
	rm -f $(APPS)
