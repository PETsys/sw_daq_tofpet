SRCS := $(wildcard Common/*cpp) $(wildcard Core/*cpp) $(wildcard TOFPET/*cpp) $(wildcard ENDOTOFPET/*cpp) $(wildcard STICv3/*cpp) $(wildcard DSIPM/*cpp)
BSRCS := $(wildcard Apps/*cpp)


CXXFLAGS ?= -g -O2
LIBS := $(LIBS) -lpthread -lrt
CXXFLAGS := $(CXXFLAGS)  -fPIC
CPPFLAGS := $(CPPFLAGS) -I.

CPPFLAGS := $(CPPFLAGS)

LIBS := $(LIBS) $(GLIBS) -L$(shell root-config --libdir --libs) $(shell root-config --auxlibs) -lMinuit -lboost_filesystem 
CPPFLAGS := $(CPPFLAGS) -I$(shell root-config --incdir)

CPPFLAGS := $(CPPFLAGS) -I../daqd/

OBJS := $(SRCS:=.o)
BOBJS := $(BSRCS:=.o)
DEPS := $(SRCS:=.d) $(BSRCS:=.d)

APPS := $(foreach APP,$(BSRCS:.cpp=), $(subst Apps/,,$(APP)))
BINARIES := $(BSRCS:=.b)

CHANNEL_MAPS := TOFPET/FEBA_PAB.map TOFPET/MEZ1_PAB.map TOFPET/FEBA_PAB_MEZ1.map TOFPET/FEBA_PAB_MEZ2.map


default: $(APPS) $(CHANNEL_MAPS)
	
$(APPS): $(BINARIES)
	
	cp -f Apps/$@.cpp.b $@

ifneq ($(MAKECMDGOALS),clean)
     include $(DEPS)
endif

%.cpp.o: %.cpp.d
	@echo Compiling $<
	$(CXX) -c -o $@ $(CXXFLAGS) $(CPPFLAGS) $(@:.o=) 

%.cpp.d: %.cpp
	@echo Generating dependencies for $<
	$(CPP) -M -MG -MT $@ -o $@ $(CPPFLAGS) $< 
	
%.cpp.b: %.cpp.o $(OBJS)
	@echo Linking $@
	$(CXX) -o $@ $< $(OBJS) $(CXXFLAGS) $(CPPFLAGS) $(LIBS)	

TOFPET/FEBA_PAB.map:	TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB.map.py
	python TOFPET/FEBA_PAB.map.py TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB.map

TOFPET/FEBA_PAB_MEZ1.map: TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB_MEZ1.map.py
	python TOFPET/FEBA_PAB_MEZ1.map.py TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB_MEZ1.map

TOFPET/FEBA_PAB_MEZ2.map: TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB_MEZ2.map.py
	python TOFPET/FEBA_PAB_MEZ2.map.py TOFPET/FEBA_STANDALONE.tsv TOFPET/FEBA_PAB_MEZ2.map

TOFPET/MEZ1_PAB.map:	TOFPET/MEZ1_STANDALONE.tsv TOFPET/MEZ1_PAB.map.py
	python TOFPET/MEZ1_PAB.map.py TOFPET/MEZ1_STANDALONE.tsv TOFPET/MEZ1_PAB.map

	
.SECONDARY: $(DEPS) $(OBJS) $(BOBJS) $(BINARIES)
.PHONY: clean
clean:
	find -type f -name '*.cpp.d' -delete
	find -type f -name '*.cpp.o' -delete
	find -type f -name '*.cpp.b' -delete
	rm -f $(APPS)
	rm -f $(CHANNEL_MAPS)
