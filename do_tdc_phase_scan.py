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
nominal_m = 128

#### PLL based generator
Generator = 1
M = 0x348	# 80 MHz PLL, 80 MHz ASIC
M = 392		# 160 MHz PLL, 160 MHz ASIC
#M = 2*392	# 160 MHz PLL, 80 MHz ASIC 
K = 19
minEventsA = 1000
minEventsB = 300

###


##### 560 MHz clean clock based generator
#Generator = 0
#M = 14	# 2x due to DDR
#K = 2 	# DO NOT USE DDR for TDCA calibration, as rising/falling is not symmetrical!
#minEvents = 1000 # Low jitter, not so many events
#####



Nmax = 8
tpLength = 900


rootFileName = argv[1]

vbias = 5
if argv[2] == "tdca":
  tdcaMode = True
  vbias = 5
  frameInterval = 0
  invertTP = False
	

elif argv[2] == "fetp":
  tdcaMode = False
  tpDAC = int(argv[3])
  vbias = float(argv[4])
  frameInterval = 16
  invertTP = True

else:
  print "Unkown mode!"
  exit(1)



if vbias > 50: minEventsA *= 10
if vbias > 50: minEventsB *= 10

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.initialize()


rootFile = ROOT.TFile(rootFileName, "RECREATE");
rootData1 = DataFile( rootFile, "3")
rootData2 = DataFile( rootFile, "3B")

activeChannels = [ y for y in range(64) ]
#activeChannels = [0, 2, 4, 6, 7, 9, 17, 23, 38, 49, 54, 63]
#activeChannels = [0, 6, 9, 15, 23,  49, 58, 63]
#activeChannels = [20]
activeAsics =  [ x for x in range(2) ]


for c in range(8):
  uut.setHVDAC(c, vbias)

minEventsA *= len(activeAsics)
minEventsB *= len(activeAsics)

hTPoint = ROOT.TH1F("hTPoint", "hTPoint", 64, 0, 64)


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
			
			# Both TDC branches
			atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "11" + "1" + "1") 
			
			# TDC E branch only
			#atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "01" + "1" + "1") 
		else:				
			atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
			atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')

	uut.config = atbConfig
	uut.uploadConfig()


	nBins = int(Nmax * M / K)
	binWidth = float(K) / M

	print nBins, binWidth
	sumProfile = ROOT.TProfile("pAll_%02d" % tChannel, "ALL", nBins, 0, nBins*binWidth, "s")
	hTFine = [[ ROOT.TH2F("htFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nBins, 0, nBins*binWidth, 1024, 0, 1024) for tac in range(4) ] for tAsic in activeAsics ]
	hEFine = [[ ROOT.TH2F("heFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nBins, 0, nBins*binWidth, 1024, 0, 1014) for tac in range(4) ] for tAsic in activeAsics ]


	uut.doSync()

	step1 = 1
	for bin in range(nBins):
		phaseStep =  bin * K
		step2 = float(phaseStep)/M + binWidth/2
		step1 = 0

		if Generator == 1:
			## Internal PLL 
			tpCoarsePhase = 0
			tpFinePhase = phaseStep
		else:
			# External fast clean clock
			tpCoarsePhase = phaseStep/M
			tpFinePhase = phaseStep % M

		uut.setTestPulsePLL(tpLength, frameInterval, tpFinePhase, invertTP)
		uut.doSync()
		t0 = time()
		
		print "Channel %02d acquiring %1.4f (%d)  @ %d" % (tChannel, step2, phaseStep, frameInterval)

		uut.start(1)
		nReceivedEvents = 0
		nAcceptedEvents = 0
		nReceivedFrames = 0
		while nAcceptedEvents < minEventsA and nReceivedFrames < (1.2 * minEventsA * (frameInterval+1)):
			decodedFrame = uut.getDataFrame()

			nReceivedFrames += 1
			
			for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
				nReceivedEvents += 1
				#print decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime
				if asic not in activeAsics or channel != tChannel:
					continue;
				
				nAcceptedEvents += 1				
			
				rootData1.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)
				
				hTFine[asic][tac].Fill(step2, tFine)
				hEFine[asic][tac].Fill(step2, eFine)
				sumProfile.Fill(step2, tFine)
				
		print "Got %(nReceivedFrames)d frames" % locals()
		print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()
		uut.start(2)

	rootData1.write()


	edgesX = [ -1.0 for x in activeAsics ]

	for n in range(len(activeAsics)):

		minADCY = 1024
		minADCX = 0
		minADCJ = 0

		h = hTFine[n][0]
		p =  h.ProfileX(h.GetName()+"_px", 1, -1, "s")
	
		for j in range(1, nBins+1):
			y = p.GetBinContent(j);
			e = p.GetBinError(j);
			x = p.GetBinCenter(j)

			#print  "PREV", minADCJ, minADCX, minADCY
			#print  "CURR", j, x, y, e

			if x < 1.0: continue

			if(e > 5.0): continue
			if(y < 0.5 * nominal_m): break

			if y < minADCY:
				minADCY = y
				minADCJ = j
				minADCX = x

		if minADCJ == 0: continue
		
		print "Found min ADC point at %f, %f" % (minADCX, minADCY)

		edgesX[n] = minADCX

	if min(edgesX) == -1: continue

	
	intervals = [ x for x in range(frameInterval, 1000, 40) ]
	nIntervals = len(intervals)

	hTFine = [[ ROOT.TH2F("htFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1024) for tac in range(4) ] for tAsic in activeAsics ]
	hEFine = [[ ROOT.TH2F("heFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1014) for tac in range(4) ] for tAsic in activeAsics ]


	for nStep, stepDelta in enumerate([-0.2, 0.8, 1.5]): # Pick 3 points for the scan
		phaseStep = sumProfile.FindBin(min(edgesX) + stepDelta) * K 
		step2 = float(phaseStep)/M + binWidth/2
		#print minADCX, nStep, stepDelta, phaseStep, step2

		if nStep == 0:
			hTPoint.SetBinContent(tChannel+1, step2);


		for interval in intervals:		
			step1 = interval+1

			if Generator == 1:
				## Internal PLL 
				tpCoarsePhase = 0
				tpFinePhase = phaseStep
			else:
				# External fast clean clock
				tpCoarsePhase = phaseStep/M
				tpFinePhase = phaseStep % M

			uut.setTestPulsePLL(tpLength, interval, tpFinePhase, invertTP)
			uut.doSync()
			t0 = time()
			
			print "Channel %02d acquiring %1.4f (%d) @ %d" % (tChannel, step2, tpFinePhase, interval)

			uut.start(1)
			nReceivedEvents = 0
			nAcceptedEvents = 0
			nReceivedFrames = 0
			while nAcceptedEvents < minEventsB and nReceivedFrames < (1.2 * minEventsB * (frameInterval+1)):
				decodedFrame = uut.getDataFrame()
				nReceivedFrames += 1

				for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
						nReceivedEvents += 1
						#print decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime
						if asic not in activeAsics or channel != tChannel:
							continue;
						
						nAcceptedEvents += 1				
					
						rootData2.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)
						
						if nStep == 0:
							hTFine[asic][tac].Fill(step1, tFine)
							hEFine[asic][tac].Fill(step1, eFine)
					
			print "Got %(nReceivedFrames)d frames" % locals()
			print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()
			uut.start(2)


		rootData2.write()




rootData1.close()
rootData2.close()
rootFile.Write()
rootFile.Close()

for c in range(8):
  uut.setHVDAC(c, 0)
