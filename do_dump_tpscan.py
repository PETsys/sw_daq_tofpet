# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from sys import argv
from bitarray import bitarray
from math import floor

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


## Move to atb.py
for ac in uut.config.asicConfig:
#	ac.globalConfig.setValue("vib2", 0)
	for cc in ac.channelConfig:
		cc.setValue("deadtime", 3);
		#cc.setValue("vbl", 56)
		cc.setValue("sh", 0)
		cc.setValue("praedictio", 1)


#for step1 in range(59,64):
tpDAC = 62 # Amplitude arbitraria (de 0 -maximo a 63 - minimo)

for step1 in [20]:	
	for tAsic in range(2):
		tChannel=step1 # Numeros dos canais a ser testados
		uut.config.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)
		uut.config.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
		uut.config.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1') #enable e configura a amplitude
		b = uut.config.asicConfig[tAsic].channelConfig[tChannel].getBaseline()
		threshold = int(b)
		uut.config.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", threshold-2)
		uut.config.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_E", threshold-2)
		uut.config.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1);
#		uut.config.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_tmjitter", 1);
		
	

	uut.uploadConfig()

	M = 392 # unidade do ajuste de fase (corresponde a um periodo do clock)

	for step2 in range(0, 8*M, 32):
		# Set the Test Pulse
		uut.setExternalTestPulse(1023, # Pulse lentgh in clock period units 
					 128, # Interval: (step1 + 1) frames
					 0, # 0!
					 step2, # Delay (unit = 1/392 clock)
					 1)

		uut.doSync() # check that the data is coming with this configuration
		print "Acquiring step %d %d..." % (step1, step2)
		uut.acquire(step1, step2, acquisitionTime)
	

for c in range(8):
	uut.config.hvBias[c] = 0

uut.uploadConfig()
#uut.setExternalTestPulse(0, 0, 0, 0, 1)
