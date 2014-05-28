# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from sys import argv
from bitarray import bitarray

# ASIC clock period
T = 6.25E-9

# Coincidence window to use be used for preliminary event selection
cWindow = 0 # 0 causes all events to be accepted
#cWindow = 25E-9

acquisitionTime = float(argv[1])
dataFilePrefix = argv[2]

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.initialize()
uut.config = loadLocalConfig()
uut.openAcquisition(dataFilePrefix, cWindow)

# Set all HV DAC channels 
for c in range(8):
	uut.config.hvBias[c] = 67.5

## Move to atb.py
for ac in uut.config.asicConfig:
	for cc in ac.channelConfig:
		cc.setValue("deadtime", 3);
		#cc.setValue("sh", 3)
		#cc.setValue("praedictio", 0)

#tChannel = 10
#tpDAC = 32
#for tAsic in range(2):
	#uut.config.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)
	#uut.config.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
	#uut.config.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')
#for c in range(8):
	#uut.config.hvBias[c] = 50.0


uut.uploadConfig()

M = 392
for step1 in range(4,1000,160):
  for step2 in range(0, 8*M, 37):
	# Set the Test Pulse
	uut.setExternalTestPulse(512, # Pulse lentgh
		step1, # Interval: (step1 + 1) frames
		0, # 0!
		step2, # Delay (unit = 1/392 clock)
		1)

	uut.doSync()
	print "Acquiring step %d %d..." % (step1, step2)
	uut.acquire(step1, float(step2)/M, acquisitionTime)
  

# Set SiPM bias to zero when finished!
for c in range(8):
	uut.config.hvBias[c] = 0
uut.uploadConfig()
uut.setExternalTestPulse(0, 0, 0, 0, 1)