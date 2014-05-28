# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from sys import argv

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
	uut.config.hvBias[c] = 67.25

for ac in uut.config.asicConfig:
	for cc in ac.channelConfig:
		cc.setValue("deadtime", 3);
		#cc.setValue("sh", 3)
		#cc.setValue("praedictio", 0)


for step1 in [0]:
  for step2 in [0]:
	# Actually upload config into hardware

	uut.uploadConfig()
	uut.doSync()
	print "Acquiring step %d %d..." % (step1, step2)
	uut.acquire(step1, step2, acquisitionTime)
  

# Set SiPM bias to zero when finished!
for c in range(8):
	uut.config.hvBias[c] = 0
uut.uploadConfig()
