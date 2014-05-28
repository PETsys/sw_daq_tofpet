SRCS := $(wildcard Common/*cpp) $(wildcard Core/*cpp) $(wildcard TOFPET/*cpp)
BSRCS := $(wildcard Apps/*cpp)


CXXFLAGS ?= -g -O2
LIBS := $(LIBS) -lpthread -lrt
CXXFLAGS := $(CXXFLAGS)  -fPIC
CPPFLAGS := $(CPPFLAGS) -I.

LIBS := $(LIBS) $(GLIBS) -L$(shell root-config --libdir --libs) $(shell root-config --auxlibs) -lMinuit 
CPPFLAGS := $(CPPFLAGS) -I$(shell root-config --incdir)

CPPFLAGS := $(CPPFLAGS) -I../daqd/

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
	@echo Comping $<
	$(CXX) -c -o $@ $(CXXFLAGS) $(CPPFLAGS) $(@:.o=) 

%.cpp.d: %.cpp
	@echo Generating dependencies for $<
	$(CPP) -M -MG -MT $@ -o $@ $(CPPFLAGS) $< 
	
%.cpp.b: %.cpp.o $(OBJS)
	@echo Linking $@
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS) $(CPPFLAGS) $(LIBS)	
	
.SECONDARY: $(DEPS) $(OBJS) $(BOBJS) $(BINARIES)
.PHONY: clean
clean:
	rm -f $(DEPS) 
	rm -f $(OBJS) $(BOBJS) 
	rm -f $(BINARIES) 
	rm -f $(APPS)
