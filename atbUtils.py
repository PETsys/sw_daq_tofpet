# -*- coding: utf-8 -*-
import cPickle as pickle
import re
import json
import sticv3


## @package atbUtils
#  This module contains auxiliary functions to load/write configurations from/to files  


## Writes the configuration of a Mezzanine/FEBA with asics in range [asicStart, asicEnd[ into a file in binary format
# @param boardConfig the configuration to be saved. It should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID of the configuration to be written
# @param asicEnd  The maximium ASIC ID (excluded) of the configuration to be written 
# @param fileName The name of the file in which to save the configuration
def dumpAsicConfig(boardConfig, asicStart, asicEnd, fileName):
	f = open(fileName, "w")
	pickler = pickle.Pickler(f, pickle.HIGHEST_PROTOCOL)
	pickler.dump(boardConfig.asicConfig[asicStart:asicEnd])
	f.close() 
	


def loadSTICv3AsicConfig(boardConfig, asic, fileName):
	#deprecated, used with old sticv3 AsicConfig
	#sticv3_config reads plain text configuration file and generates the correct bit pattern
	asicConfig = sticv3.AsicConfig(fileName)
	boardConfig.asicConfig[asic] = asicConfig


## Loads the configuration of a Mezzanine/FEBA with asics in range [asicStart, asicEnd[ from a file in binary format
# @param boardConfig The configuration in which to load. It should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID of the configuration to be loaded
# @param asicEnd  The maximium ASIC ID (excluded) of the configuration to be loaded 
# @param fileName The name of the file from which to load the configuration
# @param invert If set to true, the configuration file will be read in reverse order
def loadAsicConfig(boardConfig, asicStart, asicEnd, fileName, invert=False):
	print "Loading %s for ASICs [%d .. %d[" % (fileName, asicStart, asicEnd)
	f = open(fileName, "r")
	unpickler = pickle.Unpickler(f)
	storedConfig =  unpickler.load()
	if type(storedConfig) == list:
		# New style configuration data: N ASIC per file
		assert len(storedConfig) == (asicEnd - asicStart)
		boardConfig.asicConfig[asicStart:asicEnd] = storedConfig
		if(invert==True):
			boardConfig.asicConfig[asicStart:asicEnd].reverse()
		boardConfig.asicConfigFile[asicStart:asicEnd] = [ fileName for x in range(asicStart,asicEnd) ]
	else:
		# Old style configuration data: 1 ASIC per file
		boardConfig.asicConfig[asicStart] = storedConfig
		boardConfig.asicConfigFile[asicStart] = fileName
	f.close()

## Loads the parameters for calibration of the HV DACs from a text file
# @param boardConfig The configuration in which to load. It should be of type atb.BoardConfig
# @param start The minimum HV DAC channel ID of the configuration to be loaded
# @param end  The maximium HV DAC channel ID (excluded) of the configuration to be loaded 
# @param fileName The name of the file from which to load the calibration
def loadHVDACParams(boardConfig, start, end, fileName):
	print "Loading %s for DAC" % fileName
	boardConfig.HVDACParamsFile=fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for l in f:
		ch, m, b, x = r.split(l)
		m = float(m)
		b = float(b)
		ch=int(ch)
		assert (start + ch) < end
		boardConfig.hvParam[start+ch] = (m,b)
		
	f.close()
## Loads the HV DAC voltages desired for a given system form a text file
# @param boardConfig The configuration in which to load. It should be of type atb.BoardConfig
# @param start The minimum HV DAC channel ID of the configuration to be loaded
# @param end  The maximium HV DAC channel ID (excluded) for the configuration to be loaded 
# @param fileName The name of the file from which to load the HV DAC voltages
# @param offset An offset (in Volts) to be applied to the voltages loaded from the file
def loadHVBias(boardConfig, start, end, fileName, offset = 0.0):
	print "Loading %s for DAC" % fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for l in f:
		ch,v, x = r.split(l)
		v = float(v)
		ch=int(ch)
		assert (start + ch) < end
		boardConfig.hvBias[start+ch] = v + offset


## Loads baseline values of a Mezzanine/FEBA with asics in range [asicStart, asicEnd[ from a text file
# @param boardConfig The configuration in which to load. It should be of type atb.BoardConfig
# @param asicStart The minimum ASIC ID for the configuration to be loaded
# @param asicEnd  he maximium ASIC ID (excluded) for the configuration to be loaded 
# @param fileName The name of the file from which to load the baseline values
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

