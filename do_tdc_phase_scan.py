# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import argv, stdout, exit
from time import time, sleep
import ROOT
from rootdata import DataFile
import serial

# Parameters
T = 6.25E-9

#### PLL based generator
Generator = 1
M = 0x348	# 80 MHz PLL, 80 MHz ASIC
M = 392		# 160 MHz PLL, 160 MHz ASIC
#M = 2*392	# 160 MHz PLL, 80 MHz ASIC 
K = 27
minEvents = 10000
minEvents = 2000
###


##### 560 MHz clean clock based generator
#Generator = 0
#M = 14	# 2x due to DDR
#K = 2 	# DO NOT USE DDR for TDCA calibration, as rising/falling is not symmetrical!
#minEvents = 1000 # Low jitter, not so many events
#####



Nmin = 0
Nmax = 8
maxAcquisitionTime = 2 * minEvents / 1000 
tpLength = 512


rootFileName = argv[1]

vbias = 5
if argv[2] == "tdca":
  tdcaMode = True
  vbias = 5
  frameInterval = 0
	

elif argv[2] == "fetp":
  tdcaMode = False
  tpDAC = int(argv[3])
  vbias = float(argv[4])
  frameInterval = 4

else:
  print "Unkown mode!"
  exit(1)

if vbias > 20: minEvents *= 10

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.initialize()



rootData = DataFile(rootFileName)

activeChannels = [ y for y in range(64) ]
activeChannels = [0, 2, 4, 6, 7, 9, 17, 23, 38, 49, 54, 63]
activeAsics =  [ x for x in range(2) ]


for c in range(8):
  uut.setHVDAC(c, vbias)

minEvents *= len(activeAsics)
for tChannel in activeChannels:
	atbConfig = loadLocalConfig()
	for tAsic in activeAsics:
		atbConfig.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)

	for tAsic in activeAsics:
		# Overwrite test channel config
		tChannelConfig = atbConfig.asicConfig[tAsic].channelConfig[tChannel]
		tChannelTConfig = atbConfig.asicConfig[tAsic].channelTConfig[tChannel]

		atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1)
		if tdcaMode:
			
			atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "11" + "1" + "1") 
		else:				
			atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
			atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')

	uut.config = atbConfig
	uut.uploadConfig()


	nBins = (Nmax - Nmin) * M / K
	pTFine = [[ ROOT.TH2F("htFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nBins, Nmin, Nmax, 1024, 0, 1024) for tac in range(4) ] for tAsic in activeAsics ]
	pEFine = [[ ROOT.TH2F("heFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nBins, Nmin, Nmax, 1024, 0, 1014) for tac in range(4) ] for tAsic in activeAsics ]


	uut.doSync()

	step1 = 1
	for phaseStep in range(Nmin*M, Nmax*M, K):

		ta = time()
		b = 1/float(M)
		step1 = 0
		step2 = (phaseStep + 0.5 ) * b


		if Generator == 1:
			## Internal PLL 
			tpCoarsePhase = 0
			tpFinePhase = phaseStep
		else:
			# External fast clean clock
			tpCoarsePhase = phaseStep/M
			tpFinePhase = phaseStep % M


		## Disable pulse and wait for empty frames
##		print "Waiting (1)..."
		#uut.setExternalTestPulse(0, frameInterval, tpCoarsePhase, tpFinePhase, Generator)
		#tb = time()
		#while True:
			#for x in range(4):
				#rawIndexes = uut.getDataFramesByRawIndex(128)
				#uut.returnDataFramesByRawIndex(rawIndexes)
			#frames = [ uut.getDataFrame() for x in range(32*(2**frameInterval)) ]
			#maxEvents = max([ len(frame['events']) for frame in frames ])
			#losts = [ frame['lost'] for frame in frames ]

			#if maxEvents == 0 and True not in losts: 
				#break;
			

		#tc = time()

##		print "Waiting (2)..."
		uut.setExternalTestPulse(tpLength, frameInterval, tpCoarsePhase, tpFinePhase, Generator)
		uut.doSync()
		t0 = time()

		#td = time()

		#while (time() - t0) < (maxAcquisitionTime + 1.0):
			#for x in range(4):
				#rawIndexes = uut.getDataFramesByRawIndex(128)
				#uut.returnDataFramesByRawIndex(rawIndexes)
			#frames = [ uut.getDataFrame() for x in range(32*(2**frameInterval)) ]
			#maxEvents = max([ len(frame['events']) for frame in frames ])
			#if maxEvents > 0: 
				#break;
			

		#te = time()
					
		
		print "Channel %02d acquiring %1.4f (%d)" % (tChannel, step2, phaseStep)

		nReceivedEvents = 0
		nAcceptedEvents = 0
		nReceivedFrames = 0
		while nAcceptedEvents < minEvents and (time() - t0) < (maxAcquisitionTime + 1.0):		
			decodedFrame = uut.getDataFrame()

			nReceivedFrames += 1
			
			for asic, channel, tac, tCoarse, eCoarse, xSoC, tEoC, eEoC, tacIdle in decodedFrame['events']:

				if tEoC >= xSoC:
					tFine = tEoC - xSoC
				else:
					tFine = 1024 + tEoC - xSoC

				if eEoC >= xSoC:
					eFine = eEoC - xSoC
				else:
					eFine = 1024 + eEoC - xSoC
				nReceivedEvents += 1
				#print decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, tacIdle
				if asic not in activeAsics or channel != tChannel:
					continue;
				
				nAcceptedEvents += 1				
			
				rootData.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, xSoC,  tEoC, eEoC)
				
				pTFine[asic][tac].Fill(step2, tFine)
				pEFine[asic][tac].Fill(step2, eFine)
				
		tf = time()
		print "Got %(nReceivedFrames)d frames" % locals()
		print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()
#		print "Pulse off: %f\nwait: %f\npulse on: %f\nwait: %f\nacquire: %f" % (tb-ta, tc-tb, td-tc, te-td, tf-te)

	rootData.write()

rootData.close()

for c in range(8):
  uut.setHVDAC(c, 0)
