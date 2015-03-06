# -*- coding: utf-8 -*-
import cPickle as pickle
import re
import json
import sticv3

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


def loadHVDACParams(boardConfig, start, end, fileName):
	print "Loading %s for DAC" % fileName
	boardConfig.HVDACParamsFile=fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for i in range(start, end):
		l = f.readline()
		m, b, x = r.split(l)
		m = float(m)
		b = float(b)
		boardConfig.hvParam[i] = (m,b)
		
	f.close()

def loadHVBias(boardConfig, start, end, fileName, offset = 0.0):
	print "Loading %s for DAC" % fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for i in range(start, end):
		l = f.readline()
		v, x = r.split(l)
		v = float(v)
		boardConfig.hvBias[i] = v + offset

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

