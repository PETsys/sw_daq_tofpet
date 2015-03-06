# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import argv, stdout, exit
from time import time, sleep
import ROOT
from rootdata import DataFile
import serial
import tofpet

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
tpLength = 128


rootFileName = argv[1]

vbias = 5
if argv[2] == "tdca":
  tdcaMode = True
  vbias = 5
  frameInterval = 0
  pulseLow = False
	

elif argv[2] == "fetp":
  tdcaMode = False
  tpDAC = int(argv[3])
  vbias = float(argv[4])
  frameInterval = 16
  pulseLow = True

else:
  print "Unkown mode!"
  exit(1)

if vbias > 50: minEventsA *= 10
if vbias > 50: minEventsB *= 10

rootFile = ROOT.TFile(rootFileName, "RECREATE");

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.config = loadLocalConfig(useBaseline=False)
uut.initialize()


rootData1 = DataFile( rootFile, "3")
rootData2 = DataFile( rootFile, "3B")

activeChannels = [ x for x in range(0,64) ]
activeAsics =  [ i for i,ac in enumerate(uut.config.asicConfig) if isinstance(ac, tofpet.AsicConfig) ]
systemAsics = [ i for i in range(len(uut.config.asicConfig)) ]

minEventsA *= len(activeAsics)
minEventsB *= len(activeAsics)

hTPoint = ROOT.TH1F("hTPoint", "hTPoint", 64, 0, 64)


for tChannel in activeChannels:
	atbConfig = loadLocalConfig(useBaseline=False)
	for c, v in enumerate(atbConfig.hvBias):
		if v is None: continue
		atbConfig.hvBias[c] = vbias


	for tAsic in activeAsics:
		# Overwrite test channel config
		atbConfig.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)
		#tChannelConfig = atbConfig.asicConfig[tAsic].channelConfig[tChannel]
		#tChannelTConfig = atbConfig.asicConfig[tAsic].channelTConfig[tChannel]
		#atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1)
		if tdcaMode:
			
			# Both TDC branches
			atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "11" + "1" + "1") 
			
			# TDC E branch only
			#atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "01" + "1" + "1") 
		else:				
			atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
			atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')

	uut.stop()
	uut.config = atbConfig
	uut.uploadConfig()
	uut.start()
	uut.doSync()


	nBins = int(Nmax * M / K)
	binWidth = float(K) / M

	print "Phase scan will be done with %d steps with a %f clock binning (%5.1f ps)" % (nBins, binWidth, binWidth * T * 1E12)
	sumProfile = ROOT.TProfile("pAll_%02d" % tChannel, "ALL", nBins, 0, nBins*binWidth, "s")
	hTFine = [[ ROOT.TH2F("htFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nBins, 0, nBins*binWidth, 1024, 0, 1024) for tac in range(4) ] for tAsic in systemAsics ]
	hEFine = [[ ROOT.TH2F("heFine_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nBins, 0, nBins*binWidth, 1024, 0, 1014) for tac in range(4) ] for tAsic in systemAsics ]



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

		uut.setTestPulsePLL(tpLength, frameInterval, tpFinePhase, pulseLow)
		#uut.start(2)
		uut.doSync()
		#uut.start(1)
		t0 = time()
		
		print "Channel %02d acquiring %1.4f (%d)  @ %d" % (tChannel, step2, phaseStep, frameInterval)

		
		nReceivedEvents = 0
		nAcceptedEvents = 0
		nReceivedFrames = 0
		t0 = time()
		while nAcceptedEvents < minEventsA and (time() - t0) < 10:
			decodedFrame = uut.getDataFrame(nonEmpty = True)
			if decodedFrame is None: continue

			nReceivedFrames += 1			
			for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
				nReceivedEvents += 1
				#print decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime
				if asic not in activeAsics or channel != tChannel:
					continue;
				#print ".. accepting ", nAcceptedEvents
				
				nAcceptedEvents += 1				
			
				rootData1.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)
				
				hTFine[asic][tac].Fill(step2, tFine)
				hEFine[asic][tac].Fill(step2, eFine)
				sumProfile.Fill(step2, tFine)
				
		print "Got %(nReceivedFrames)d frames" % locals()
		print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()

	rootData1.write()
	#continue
	
	edgesX = [ -1.0 for x in systemAsics ]
	print activeAsics
	for n in activeAsics:

		minADCY = 1024
		minADCX = 0
		minADCJ = 0
                minADCE = 1E6

		h = hTFine[n][0]
		p =  h.ProfileX(h.GetName()+"_px", 1, -1, "s")
	
		for j in range(1, nBins+1):
			y = p.GetBinContent(j);
			e = p.GetBinError(j);
			x = p.GetBinCenter(j)
			nc = p.GetBinEntries(j);
			#print  "PREV", minADCJ, minADCX, minADCY
			#print  "CURR", nc, j, x, y, e

			if nc < minEventsA/(10*len(activeAsics)) : continue # not enough events
			if x < 1.0: continue # too early
			if e < 0.10: continue # yeah, righ!
			if e > 5.0: continue # too noisy
                        if y < 0.5 * nominal_m: continue; # out of range

			if y < minADCY:
				minADCY = y
				minADCJ = j
				minADCX = x
                                minADCE = e

		if minADCJ == 0: 
			print "Min point not found for ASIC %d" % n
			continue
		
		print "ASIC %d Found min ADC point at %f, %f with RMS %f" % (n, minADCX, minADCY, minADCE)

		edgesX[n] = minADCX

	if max(edgesX) == -1: continue # Didn't find edges for any ASIC

	
	intervals = [ x for x in range(frameInterval, 1000, 40) ]
	nIntervals = len(intervals)

	hTFine = [[ ROOT.TH2F("htFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1024) for tac in range(4) ] for tAsic in systemAsics ]
	hEFine = [[ ROOT.TH2F("heFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1014) for tac in range(4) ] for tAsic in systemAsics ]


	for nStep, stepDelta in enumerate([-0.2]): # Pick points for the scan from the edge
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

			uut.setTestPulsePLL(tpLength, interval, tpFinePhase, pulseLow)
			uut.doSync()
			t0 = time()
			
			print "Channel %02d acquiring %1.4f (%d) @ %d" % (tChannel, step2, tpFinePhase, interval)

			#uut.start(1)
			nReceivedEvents = 0
			nAcceptedEvents = 0
			nReceivedFrames = 0
			t0 = time()
			while nAcceptedEvents < minEventsB and (time() - t0) < 15:
				decodedFrame = uut.getDataFrame(nonEmpty = True)
				if decodedFrame is None: continue

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
			#uut.start(2)


		rootData2.write()




rootData1.close()
rootData2.close()
rootFile.Write()
rootFile.Close()

for c in range(8):
  uut.setHVDAC(c, 0)
