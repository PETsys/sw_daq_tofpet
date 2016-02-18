# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import  stdout, exit
from time import time, sleep
import ROOT
from rootdata import DataFile
import tofpet
from os.path import dirname, isdir
import argparse

parser = argparse.ArgumentParser(description='Acquires a set of data for several relative phases of the test pulse, either injecting it directly in the tdcs or in the front end')


parser.add_argument('OutputFilePrefix',
                   help='Prefix for the output files. One pair of .root and .params files will be created')

parser.add_argument('hvBias', type=float,
                   help='The voltage to be set for the HV DACs')


parser.add_argument('--asics', nargs='*', type=int, help='If set, only the selected asics will acquire data')

parser.add_argument('--mode', type=str, required=True,choices=['tdca', 'fetp'], help='Defines where the test pulse is injected. Two modes are allowed: tdca and fetp ')

parser.add_argument('--tpDAC', type=int, default=0, help='The amplitude of the test pulse in DAC units (Default is 32 ). When running in fetp mode, this value needs to be set')

parser.add_argument('--comments', type=str, default="", help='Any comments regarding the acquisition. These will be saved as a header in OutputFilePrefix.params')


args = parser.parse_args()


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


rootFileName = "%s.root" % args.OutputFilePrefix
assert isdir(dirname(rootFileName))


if args.mode == "tdca":
  tdcaMode = True
  if args.hvBias == None:
    vbias =  5
  else:
    vbias = args.hvBias
  frameInterval = 0
  pulseLow = False
	

elif args.mode == "fetp":
  tdcaMode = False
  if args.tpDAC==0:
    print "Error: Please set tpDAC argument! Exiting..."
    exit(1)
  tpDAC = args.tpDAC

  if args.hvBias == None:
    vbias =  5
  else:
    vbias = args.hvBias
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


if args.asics == None:
  activeAsics =  uut.getActiveTOFPETAsics()
else:
  activeAsics= args.asics

activeChannels = [ x for x in range(0,64) ]
systemAsics = [ i for i in range(max(activeAsics) + 1) ]

minEventsA *= len(activeAsics)
minEventsB *= len(activeAsics)

hTPoint = ROOT.TH1F("hTPoint", "hTPoint", 64, 0, 64)

atbConfig = loadLocalConfig(useBaseline=False)
for c, v in enumerate(atbConfig.hvBias):
  atbConfig.hvBias[c] = vbias

uut.config = atbConfig
uut.config.writeParams(args.OutputFilePrefix, args.comments)

print "Active ASICs ID: ", activeAsics

for tChannel in activeChannels:
	atbConfig = loadLocalConfig(useBaseline=False)
	for c, v in enumerate(atbConfig.hvBias):
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
				#print "Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d %8d" % (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime) 
				if asic not in activeAsics or channel != tChannel:
					#print "WARNING 1: Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d %8d" % (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime) 
					continue
				
				nAcceptedEvents += 1				
			
				rootData1.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)
				
				hTFine[asic][tac].Fill(step2, tFine)
				hEFine[asic][tac].Fill(step2, eFine)
				sumProfile.Fill(step2, tFine)
				
		print "Got %(nReceivedFrames)d frames" % locals()
		print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()

	rootData1.write()
	
	histograms = [ hTFine[n][tac] for n in activeAsics for tac in range(4) ]
	if tdcaMode:
		histograms = [ hEFine[n][tac] for n in activeAsics for tac in range(4) ]
		
	profiles = [ h.ProfileX(h.GetName()+"_px", 1, -1, "s") for h in histograms ]
	
	maxE = 2.0
	minE = 0.1
	minList = [ 1024 for j in range(0, nBins+1) ]
	for j in range(1, nBins):
		try:		
			errors = [ p.GetBinError(i) for p in profiles for i in range(j-1, j+1) ]
			maxADC = max([p.GetBinContent(j) for p in profiles if min(errors) > minE and max(errors) < maxE ])
			minList[j] = maxADC			
		except ValueError as e:
			pass

	minList2 = [v for v in minList]
	for j in range(1, nBins):
		minList2[j] = max(minList[j-1:j+1])
		
	minADC = min(minList2)
	if minADC == 1024: continue # Could not find a suitable point for any channel	
	minADCJ = minList.index(minADC)
	
	
	intervals = [ x for x in range(frameInterval, 1000, 40) ]
	nIntervals = len(intervals)

	hTFine = [[ ROOT.TH2F("htFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "T Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1024) for tac in range(4) ] for tAsic in systemAsics ]
	hEFine = [[ ROOT.TH2F("heFineB_%03d_%02d_%1d" % (tAsic, tChannel, tac), "E Fine", nIntervals, frameInterval+1, frameInterval+40*nIntervals+1, 1024, 0, 1014) for tac in range(4) ] for tAsic in systemAsics ]
	
	
	minADCX = profiles[0].GetBinCenter(minADCJ)
	while minADCX > 2.0: minADCX -= 2.0

	profiles = profiles[::4] # Get only profiles for TAC 0
	for nStep, x in enumerate([minADCX]): # Old code from where we scanned multiple steps
		j = profiles[0].FindBin(x)
		phaseStep = (j - 1) * K
		step2 = float(phaseStep)/M + binWidth/2
		print "Phase selection: %5.4f clk" % (step2)
		for i, n in enumerate(activeAsics):
			print "ASIC %4d Channnel %d TAC 0 T ADC value is %5.1f mean %3.3f RMS" % (n, tChannel, profiles[i].GetBinContent(j), profiles[i].GetBinError(j))

		if nStep == 0:
			hTPoint.SetBinContent(tChannel+1, step2);


		for interval in intervals:		
			step1 = interval+1
			expectedChannelIdleTime = 1024*step1
			expectedTACIdleTime = 4 * expectedChannelIdleTime

			if Generator == 1:
				## Internal PLL 
				tpCoarsePhase = 0
				tpFinePhase = phaseStep
			else:
				# External fast clean clock
				tpCoarsePhase = phaseStep/M
				tpFinePhase = phaseStep % M

			uut.setTestPulsePLL(tpLength, interval, tpFinePhase, pulseLow)
			sleep(expectedTACIdleTime * T)
			uut.doSync()
			t0 = time()
			
			print "Channel %02d acquiring %1.4f (%d) @ %d" % (tChannel, step2, tpFinePhase, interval)

			#uut.start(1)
			nReceivedEvents = 0
			nAcceptedEvents = 0
			nReceivedFrames = 0
			t0 = time()
			#print "Expected idle times: %8d %8d" % (1024*step1, 4*1024*step1)
			while nAcceptedEvents < minEventsB and (time() - t0) < 15:
				decodedFrame = uut.getDataFrame(nonEmpty = True)
				if decodedFrame is None: continue

				nReceivedFrames += 1

				for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
						nReceivedEvents += 1
						#print "Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d %8d" % (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime) 
						if asic not in activeAsics or channel != tChannel:
							#print "WARNING 1: Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d %8d" % (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime) 
							continue

						if channelIdleTime < expectedChannelIdleTime or tacIdleTime < expectedTACIdleTime:						
							#print "WARNING 2: Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d (%8d) %8d (%8d)" \
							#	% (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, \
							#	channelIdleTime, (channelIdleTime-expectedChannelIdleTime), \
							#	tacIdleTime, (tacIdleTime-expectedTACIdleTime)) 
							continue

						if tdcaMode == False and tCoarse == 0 and eCoarse == 0:
							#print "WARNING 3: Frame %10d ASIC %3d CH %2d TAC %d (%4d %4d) (%4d %4d) %8d %8d" % (decodedFrame['id'], asic, channel, tac, tCoarse, tFine, eCoarse, eFine, channelIdleTime, tacIdleTime) 
							continue

						nAcceptedEvents += 1				
					
						rootData2.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)
						
						if nStep == 0:
							hTFine[asic][tac].Fill(step1, tFine)
							hEFine[asic][tac].Fill(step1, eFine)
					
			print "Got %(nReceivedFrames)d frames" % locals()
			print "Got %(nReceivedEvents)d events, accepted %(nAcceptedEvents)d" % locals()
			#uut.start(2)

		uut.setTestPulseNone()
		uut.doSync()

		rootData2.write()




rootData1.close()
rootData2.close()
rootFile.Write()
rootFile.Close()

uut.setAllHVDAC(0)
