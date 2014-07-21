# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import argv, stdout, maxint
from time import time, sleep
import ROOT
from rootdata import DataFile
import serial

acquisitionTime = float(argv[1])
tChannel = int(argv[2])
dataFilePrefix = argv[3]


# ASIC clock period
T = 6.25E-9

# Preliminary coincidence window
# Coincidence window to use be used for preliminary event selection
# 0 causes all events to be accepted
cWindow = 0
#cWindow = 25E-9

tpDAC = 32
tpFrameInterval = 16
tpCoarsePhase = 0
tpFinePhase = 1
tpLength = 512




atbConfig = loadLocalConfig(loadBaseline=False)
for c in range(len(atbConfig.hvBias)):
		atbConfig.hvBias[c] = 50.0

for tAsic in range(2):
	atbConfig.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)
	atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
	atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')
	#atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", 32);
	#atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1);
	#atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("praedictio", 0);

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.initialize()
uut.config = atbConfig
uut.uploadConfig()
uut.doSync()
uut.openAcquisition(dataFilePrefix, cWindow)
uut.setTestPulsePLL(tpLength, tpFrameInterval, tpFinePhase, False)

uut.config.writeParams(dataFilePrefix)

uut.doSync()
for step1 in range(0,64,4): # vib
  for step2 in range(0,64,4): #vbl
	t0 = time()
	lastWriteTime = t0
	#atbConfig.asicConfig[tAsic].globalConfig.setValue("postamp", step1)
	#atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", step2)
	#for tAsic in range(2):
		#atbConfig.asicConfig[tAsic].globalConfig.setValue("sipm_idac_dcstart", step1)
		#for c in range(64):
			#atbConfig.asicConfig[tAsic].channelConfig[c].setValue("vbl", step2)

	for ac in atbConfig.asicConfig:
		ac.globalConfig.setValue("vib1", step1);
		for cc in ac.channelConfig:
			cc.setValue("vbl", step2);

	uut.uploadConfig()
	uut.doSync()
	print "Acquiring step %d %d..." % (step1, step2)
	uut.acquire(step1, step2, acquisitionTime)
  

# Set SiPM bias to zero when finished!
for c in range(8):
	uut.setHVDAC(c, 0)
