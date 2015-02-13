# -*- coding: utf-8 -*-
import cPickle as pickle
import re
import json


## @package atbUtils
#  This module contains auxiliary functions to load/write configurations from/to files  

## Writes the configuration of a Mezzanine/FEBA into a file in binary format
# @param boardConfig the configuration to be saved, it should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID number for this board. Should be 0 or 2 for a system with two boards 
# @param asicEnd  The maximium ASIC ID number for this board. Should be 2 (1 if using 1 ASIC boards) or 4 (3 if using 1 ASIC boards) for a system with two boards 
# @param filename The name of the file in which to save the configuration
def dumpAsicConfig(boardConfig, asicStart, asicEnd, fileName):
	f = open(fileName, "w")
	pickler = pickle.Pickler(f, pickle.HIGHEST_PROTOCOL)
	pickler.dump(boardConfig.asicConfig[asicStart:asicEnd])
	f.close() 
	
## Loads the configuration of a Mezzanine/FEBA from a file 
# @param boardConfig The configuration to be  it should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID number for this board. Should be 0 or 2 for a system with two boards 
# @param asicEnd  The maximium ASIC ID number for this board. Should be 2 (1 if using 1 ASIC boards) or 4 (3 if using 1 ASIC boards) for a system with two boards  
# @param filename The name of the file in which to save the configuration
def loadAsicConfig(boardConfig, asicStart, asicEnd, fileName):
	print "Loading %s for ASICs [%d .. %d[" % (fileName, asicStart, asicEnd)
	f = open(fileName, "r")
	unpickler = pickle.Unpickler(f)
	storedConfig =  unpickler.load()
	if type(storedConfig) == list:
		# New style configuration data: N ASIC per file
		assert len(storedConfig) == (asicEnd - asicStart)
		boardConfig.asicConfig[asicStart:asicEnd] = storedConfig
		boardConfig.asicConfigFile[asicStart:asicEnd] = [ fileName for x in range(asicStart,asicEnd) ]
	else:
		# Old style configuration data: 1 ASIC per file
		boardConfig.asicConfig[asicStart] = storedConfig
		boardConfig.asicConfigFile[asicStart] = fileName
	f.close()


def loadHVDACParams(boardConfig, fileName):
	print "Loading %s for DAC" % fileName
	boardConfig.HVDACParamsFile=fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for i,v in enumerate(boardConfig.hvParam):
		l = f.readline()
		m, b, x = r.split(l)
		m = float(m)
		b = float(b)
		boardConfig.hvParam[i] = (m,b)
		
	f.close()

## Loads the baseline values of a Mezzanine/FEBA from a text file 
# @param boardConfig The configuration to be  it should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID number for this board. Should be 0 or 2 for a system with two boards 
# @param asicEnd  The maximium ASIC ID number for this board. Should be 2 (1 if using 1 ASIC boards) or 4 (3 if using 1 ASIC boards) for a system with two boards  
# @param filename The name of the file in which to save the configuration
def loadBaseline(boardConfig, asicStart, asicEnd, fileName):
	print "Loading %s for ASICs [%d .. %d[" % (fileName, asicStart, asicEnd)
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]*')
	for asic in range(asicStart, asicEnd):
		boardConfig.asicBaselineFile[asic] = fileName
		for i in range(64):
			l = f.readline()
			s =  r.split(l)
			s = s[0:4]
			#print s
			a, c, baseline, noise = s
			v = float(baseline)
			boardConfig.asicConfig[asic].channelConfig[i].setBaseline(v)
			boardConfig.asicConfig[asic].channelConfig[i].setValue("vth_T", int(round(v-14)))

		
	f.close()

