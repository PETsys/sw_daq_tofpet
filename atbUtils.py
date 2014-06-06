# -*- coding: utf-8 -*-
import cPickle as pickle
import re

def dumpAsicConfig(boardConfig, asic, fileName):
	f = open(fileName, "w")
	pickler = pickle.Pickler(f, pickle.HIGHEST_PROTOCOL)
	pickler.dump(boardConfig.asicConfig[asic])
	f.close()
	

def loadAsicConfig(boardConfig, asic, fileName):
	print "Loading %s for ASIC %d" % (fileName, asic)
	f = open(fileName, "r")
	unpickler = pickle.Unpickler(f)
	boardConfig.asicConfig[asic] = unpickler.load()
	f.close()


def loadHVDACParams(boardConfig, fileName):
	print "Loading %s for DAC" % fileName
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]+')
	for i,v in enumerate(boardConfig.hvParam):
		l = f.readline()
		m, b, x = r.split(l)
		m = float(m)
		b = float(b)
		boardConfig.hvParam[i] = (m,b)
		
	f.close()

def loadBaseline(boardConfig, asic, fileName):
	print "Loading %s for ASIC %d" % (fileName, asic)
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]*')
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

