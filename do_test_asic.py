# -*- coding: utf-8 -*-
import atb
import tofpet
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import argv, stdout, exit
from time import time, sleep
import ROOT
from rootdata import DataFile
import serial
import numpy as np
import argparse

parser = argparse.ArgumentParser(description='Acquires a set of data for 4 relative phases of the test pulse, either injecting it directly in the tdcs or/and in the front end.')


parser.add_argument('OutputFile',
                   help='Output File Prefix. The output file will contain relevant data of the ASIC(s) test. The suffix .log will be added')

parser.add_argument('--hvBias', type=float, default=5, help='The voltage to be set for the HV DACs. Relevant only to fetp mode. In tdca mode, it is set to 5V.')

parser.add_argument('--mode', type=str, choices=['tdca', 'fetp', 'both'], default='both', help='Defines where the test pulse is injected. Three modes are allowed: tdca, fetp and both. ')

parser.add_argument('--tpDAC', type=int, default=32, help='The amplitude of the test pulse in DAC units (Default is 32 )')

parser.add_argument('--multipleAsics', action='store_true', default=False, help='Shows a more detailed output for each active ASIC. Default is off, for ASIC testing, with output of GOOD or BAD ASIC")')

parser.add_argument('--writeRoot', action='store_true', default=False, help='Writes the ROOT output file based on the iutput file prefix. By default this option is set to False")')


args = parser.parse_args()


# Parameters
T = 6.25E-9
nominal_m = 140

nPhases = 5

#### PLL based generator
Generator = 1
M = 0x348	# 80 MHz PLL, 80 MHz AIC
M = 392		# 160 MHz PLL, 160 MHz ASIC
phaseStep = 2*M/nPhases; # the interval between each test pulse phase 
intervalADC=nominal_m*2/nPhases;
#M = 2*392	# 160 MHz PLL, 80 MHz ASIC 

minEventsA = 1200
minEventsB = 300

###


##### 560 MHz clean clock based generator
#Generator = 0
#M = 14	# 2x due to DDR
#K = 2 	# DO NOT USE DDR for TDCA calibration, as rising/falling is not symmetrical!
#minEvents = 1000 # Low jitter, not so many events
#####


tpLength = 128
tpFrameInterval = 16
tpCoarsePhase = 1


activeAsics= [ 0 ]

uut = atb.ATB("/tmp/d.sock", False, F=1/T)


atbConfig = atb.BoardConfig()
for i in activeAsics:
    atbConfig.asicConfig[i]=tofpet.AsicConfig()

if(args.multipleAsics):
    atbConfig = loadLocalConfig(useBaseline=False)

uut.config=atbConfig
uut.setAllHVDAC(args.hvBias)
uut.initialize()

if(args.multipleAsics):
    activeAsics = uut.getActiveTOFPETAsics()

log = open(args.OutputFile + '.log', 'w+')

if(args.writeRoot):
    rootFile = ROOT.TFile(args.OutputFile + '.root', "RECREATE")
    rootData1 = DataFile( rootFile, "3")
    rootData2 = DataFile( rootFile, "3B")


activeChannels = [ x for x in range(0,64) ]


maxAsics=max(activeAsics) + 1
systemAsics = [ i for i in range(maxAsics) ]

#minEventsA *= len(activeAsics)
#minEventsB *= len(activeAsics)


# Initialize variables for flaging an ASIC as good or BAD 
flagMaybe = False
flagBad = False

# Initialize arrays to store the relevant data for each TAC and test pulse mode
deadChannels = np.zeros((2,maxAsics,64,4))
TFineRMS = np.zeros((2,maxAsics,64,4))
ToT = np.zeros((2,maxAsics,64,4))
EFineRMS = np.zeros((2,maxAsics,64,4))
phaseProblemsT = np.zeros((2,maxAsics,64,4))
phaseProblemsE = np.zeros((2,maxAsics,64,4))

# Initialize histograms to store the data from test pulse for each TAC and test pulse phase 
hTFine = [[[[ ROOT.TH1F("htFine_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "T Fine", 1024, 0, 1024) for finephase in range(nPhases)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]
hEFine = [[[[ ROOT.TH1F("heFine_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "E Fine", 1024, 0, 1014) for finephase in range(nPhases)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]
hToT = [[[[ ROOT.TH1F("heToT_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "Coarse ToT", 1024, 0, 1014) for finephase in range(nPhases)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]


nmodes=0
if args.mode == "tdca" or args.mode == "fetp":
    nmodes=1
if args.mode == "both":
    nmodes=2
  



for iteration in range(nmodes):
   

	if (args.mode == "tdca" and iteration==0) or (args.mode == "both" and iteration==0):
		mode=0
		tdcaMode = True
		vbias =  5
		frameInterval = 0
		pulseLow = False
	

	elif (args.mode == "fetp" and iteration ==0) or (args.mode == "both" and iteration ==1):
		mode=1
		tdcaMode = False
		tpDAC = args.tpDAC
		if args.hvBias == None:
			vbias =  5
		else:
			vbias = args.hvBias
			frameInterval = 16
		pulseLow = True

	for  asic in range(len(systemAsics)):
		for  channel in range(64):
			for  tac in range(4):
				for  finephase in range(nPhases):
					hTFine[asic][channel][tac][finephase].Reset()
					hEFine[asic][channel][tac][finephase].Reset()
					hToT[asic][channel][tac][finephase].Reset()


	for tChannel in activeChannels:
			
		nEvents=0	
		
		if vbias > 50: minEventsA *= 10

              
		for i in range(nPhases):
			tpFinePhase=phaseStep*i + 1
                        if(args.multipleAsics):
                            if(tdcaMode):
				stdout.write("Running for Channel %02d; Test pulse fine phase value = %03d; mode TDCA\n" %(tChannel, tpFinePhase))
                                stdout.flush()
                            else:
				stdout.write("Running for Channel %02d; Test pulse fine phase value = %03d; mode FETP\n" %(tChannel, tpFinePhase))
                                stdout.flush()
                        else:
                            if(tdcaMode):
				stdout.write("Running for Channel %02d; Test pulse fine phase value = %03d; mode TDCA\r" %(tChannel, tpFinePhase))
                                stdout.flush()
                            else:
				stdout.write("Running for Channel %02d; Test pulse fine phase value = %03d; mode FETP\r" %(tChannel, tpFinePhase))
                                stdout.flush()
                                
                        atbConfig = atb.BoardConfig()
                        for tAsic in activeAsics:
                            atbConfig.asicConfig[tAsic]=tofpet.AsicConfig()

                        if(args.multipleAsics):
                            atbConfig = loadLocalConfig(useBaseline=False)
		

			for tAsic in activeAsics:
                            atbConfig.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)
                            atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1)
                            if tdcaMode:

                                    # Both TDC branches
                                atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "11" + "1" + "1") 


                            else:				
                                atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
                                atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')


            
			uut.config = atbConfig
			uut.uploadConfig()
			
			uut.setTestPulsePLL(tpLength, tpFrameInterval, tpFinePhase, pulseLow)
			uut.doSync()

        
			t0 = time()


 
           
			nReceivedEvents = 0
			nAcceptedEvents = 0
			nReceivedFrames = 0
                       

                        while nReceivedFrames < minEventsA and (time() - t0) < 1:
			#while nAcceptedEvents < minEventsA and (time() - t0) < 1:

				decodedFrame = uut.getDataFrame(nonEmpty=True)
				if decodedFrame is None: continue

				nReceivedFrames += 1

				for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
					if channel == tChannel:
						nReceivedEvents += 1
 
					     
					     #    if tdcaMode==False:
					     #       print asic, tac, tCoarse, eCoarse, eCoarse-tCoarse,  6.25*(eCoarse-tCoarse)
						nAcceptedEvents += 1				
						nEvents += 1	
 

						hTFine[asic][channel][tac][i].Fill(tFine)
						hEFine[asic][channel][tac][i].Fill(eFine)
						hToT[asic][channel][tac][i].Fill((eCoarse-tCoarse)*T*1E9)

					     
                        #print "Channel %(tChannel)d: Got %(nReceivedEvents)d events in  %(nReceivedFrames)d frames, accepted %(nAcceptedEvents)d" % locals()

                #Write data into log                               
                for tAsic in activeAsics:
                    for tac in range(4):
                        for phase in range(nPhases):
                            log.write("%d\t%d\t%d\t%d\t%d\t%d\t%f\t%f\t%f\t%f\t%f\n" % (mode,tAsic, tChannel,tac, phase, hTFine[tAsic][tChannel][tac][phase].GetEntries(), hTFine[tAsic][tChannel][tac][phase].GetRMS(), hEFine[tAsic][tChannel][tac][phase].GetRMS(), hTFine[tAsic][tChannel][tac][phase].GetMean(), hEFine[tAsic][tChannel][tac][phase].GetMean(), hToT[tAsic][tChannel][tac][phase].GetMean()))
                            

                #Start searching for problems
                for tAsic in activeAsics:
                    for tac in range(4):
                        #Search the max number of entries
                        entriesArr=[hTFine[tAsic][tChannel][tac][0].GetEntries(), hTFine[tAsic][tChannel][tac][1].GetEntries() , hTFine[tAsic][tChannel][tac][2].GetEntries(), hTFine[tAsic][tChannel][tac][3].GetEntries(),hTFine[tAsic][tChannel][tac][4].GetEntries()]
                        entries = max(entriesArr)        
                        if entries == 0:
                            deadChannels[mode][tAsic][tChannel][tac]=1
                            

                        #Search 2nd highest RMS         
                        tRMSarr = sorted([hTFine[tAsic][tChannel][tac][0].GetRMS(), hTFine[tAsic][tChannel][tac][1].GetRMS(), hTFine[tAsic][tChannel][tac][2].GetRMS(), hTFine[tAsic][tChannel][tac][3].GetRMS(), hTFine[tAsic][tChannel][tac][4].GetRMS()])                    
                        tRMS = tRMSarr[2]                        
                        eRMSarr = sorted([hEFine[tAsic][tChannel][tac][0].GetRMS(), hEFine[tAsic][tChannel][tac][1].GetRMS(), hEFine[tAsic][tChannel][tac][2].GetRMS(), hEFine[tAsic][tChannel][tac][3].GetRMS(), hEFine[tAsic][tChannel][tac][4].GetRMS()])      
                        eRMS = eRMSarr[2]
                        
                        
                     
                        
                        meanPhaseTarr=[hTFine[tAsic][tChannel][tac][0].GetMean(), hTFine[tAsic][tChannel][tac][1].GetMean(), hTFine[tAsic][tChannel][tac][2].GetMean(), hTFine[tAsic][tChannel][tac][3].GetMean(), hTFine[tAsic][tChannel][tac][4].GetMean()]
                        meanPhaseEarr=[hEFine[tAsic][tChannel][tac][0].GetMean(), hEFine[tAsic][tChannel][tac][1].GetMean(), hEFine[tAsic][tChannel][tac][2].GetMean(), hEFine[tAsic][tChannel][tac][3].GetMean(), hEFine[tAsic][tChannel][tac][4].GetMean()]

                        nProblemsT=0
                        nProblemsE=0
                  

                        for j in range(nPhases-2):
                            diffT = meanPhaseTarr[j]-meanPhaseTarr[j+1]   
                            diffE = meanPhaseEarr[j]-meanPhaseEarr[j+1]  
                            
                            if(diffT > intervalADC*1.2 or diffT < intervalADC*0.8):
                                    nProblemsT += 1
                                    #print diffT, tac, j, "Time", nProblemsT
                            if((diffE > intervalADC*1.2 or diffE < intervalADC*0.8) and mode == 0):
                                    nProblemsE += 1
                                    #print diffE, tac, j, "Energy", nProblemsE
                        diffT = meanPhaseTarr[nPhases-1]-meanPhaseTarr[0]   
                        diffE = meanPhaseEarr[nPhases-1]-meanPhaseEarr[0]
                        if(diffT > intervalADC*1.2 or diffT < intervalADC*0.8):
                            nProblemsT += 1
                            #print diffT, tac, j, "Time", nProblemsT
                        if((diffE > intervalADC*1.2 or diffE < intervalADC*0.8) and mode == 0):
                            nProblemsE += 1
                            #print diffE, tac, j, "Energy", nProblemsE


                        #Search for the maximum TOT in the 4 phases        
                        coarseTOT=max(hToT[tAsic][tChannel][tac][0].GetMean(), hToT[tAsic][tChannel][tac][1].GetMean(), hToT[tAsic][tChannel][tac][2].GetMean(), hToT[tAsic][tChannel][tac][3].GetMean(), hToT[tAsic][tChannel][tac][4].GetMean())


                        phaseProblemsT[mode][tAsic][tChannel][tac]=nProblemsT
                        phaseProblemsE[mode][tAsic][tChannel][tac]=nProblemsE
                        TFineRMS[mode][tAsic][tChannel][tac]=tRMS
                        EFineRMS[mode][tAsic][tChannel][tac]=eRMS
                        ToT[mode][tAsic][tChannel][tac]=coarseTOT

			# Adjustment for different clock frequencies
			# TDC RMS needs to be adjusted by this
			# ToT are already in ns
			k = T/6.25E-9

                        if(entries<100 or nProblemsT>2 or nProblemsE>2 or (tRMS*k>4 and mode==1) or (eRMS*k>3 and mode==0) or (tRMS*k>1.5 and mode==0) or (coarseTOT <50 and mode==1)):
                            flagBad=True;
                            
                           
                           
                    
          


if(args.multipleAsics):
    for tAsic in activeAsics:
        nDeadTAC=0
        nDeadFETP=0
        nTTDCMean=0
        nETDCMean=0
        nTTDCRms=0
        nETDCRms=0
        nTFETPMean=0
        nEFETPMean=0
        nMIRms=0
        nMIToT=0
        nNURms=0
        nNUToT=0
        print "\n\n############ Report for ASIC %d ############\n\n" % tAsic
        for tChannel in activeChannels:
            continue_flag=0    
            if any(deadChannels[0][tAsic][tChannel][:])==1 and args.mode!="fetp":
                print "Channel %d: DEAD on TDCA" % tChannel
                nDeadTAC+=1
                continue

            if any(deadChannels[1][tAsic][tChannel][:])==1 and args.mode!="tdca":
                print "Channel %d: DEAD on FETP" % tChannel
                nDeadFETP+=1
                continue

            if args.mode!="fetp":
                for i in range(4):
                    if phaseProblemsT[0][tAsic][tChannel][i] > 2:
                        print "Channel %d: Problems on phase T fine measures in TDCA: %d problems found for at least one TAC" % (tChannel, phaseProblemsT[0][tAsic][tChannel][i])
                        nTTDCMean+=1
                        continue_flag=1
                        break
                if(continue_flag):
                    continue

                for i in range(4):
                    if phaseProblemsE[0][tAsic][tChannel][i] > 2:
                        print "Channel %d: Problems on phase E fine measures in TDCA: %d problems found for at least one TAC" % (tChannel, phaseProblemsE[0][tAsic][tChannel][i])
                        nETDCMean+=1
                        continue_flag=1
                        break
                if(continue_flag):
                    continue

            ###################################################
                for i in range(4):
                    if TFineRMS[0][tAsic][tChannel][i]>1:
                        print "Channel %d:  High TFine RMS on TDCA: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[0][tAsic][tChannel][i])
                        nTTDCRms+=1
                        continue_flag=1
                        break
                if(continue_flag):
                    continue

            ###################################################
                for i in range(4):
                    if EFineRMS[0][tAsic][tChannel][i]>2:
                        print "Channel %d:  High EFine RMS on TDCA: %lf (MANUAL INSPECTION)" % (tChannel,EFineRMS[0][tAsic][tChannel][i])
                        nETDCRms+=1
                        continue_flag=1
                        break
                if(continue_flag):
                    continue

            if args.mode=="tdca":
                continue

           ###################################################
            for i in range(4):
                if phaseProblemsT[1][tAsic][tChannel][i] > 2:
                    print "Channel %d: Problems on phase T fine measures in FETP: %d problems found for at least one  TAC" % (tChannel, phaseProblemsT[1][tAsic][tChannel][i])
                    nTFETPMean+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue

            ###################################################
            #for i in range(4):
            #    if phaseProblemsE[1][tAsic][tChannel][i] > 2:
            #        print "Channel %d: Problems on phase E fine measures in FETP: %d problems found for at least one TAC" % (tChannel, phaseProblemsE[1][tAsic][tChannel][i])
            #        nEFETPMean+=1
            #        continue_flag=1
            #        break
            #if(continue_flag):
            #    continue

            ###################################################
            for i in range(4):
                if (TFineRMS[1][tAsic][tChannel][i]> 1 and TFineRMS[1][tAsic][tChannel][i]<2 and ToT[1][tAsic][tChannel][i]> 50 and ToT[1][tAsic][tChannel][i]<80):
                    print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][i], ToT[1][tAsic][tChannel][i])
                    nMIRms+=1
                    nMIToT+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue      

            ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]> 1 and TFineRMS[1][tAsic][tChannel][i]<2 and ToT[1][tAsic][tChannel][i]<50:
                    print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][i], ToT[1][tAsic][tChannel][i])
                    nMIRms+=1
                    nNUToT+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue 

            ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]>2 and ToT[1][tAsic][tChannel][i]> 50 and ToT[1][tAsic][tChannel][i]<80:
                    print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][i], ToT[1][tAsic][tChannel][i])
                    nNURms+=1
                    nMIToT+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue 

            ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]>2 and ToT[1][tAsic][tChannel][i]< 50:
                    print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][i], ToT[1][tAsic][tChannel][i])
                    nNURms+=1
                    nNUToT+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue 

           ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]>2 and ToT[1][tAsic][tChannel][i]>80:
                    print "Channel %d:  High RMS: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][i])
                    nNURms+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue 

           ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]>1 and TFineRMS[1][tAsic][tChannel][i]<2 and ToT[1][tAsic][tChannel][i]>80:
                    print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][i])
                    nMIRms+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue

          ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]<1 and ToT[1][tAsic][tChannel][i]< 50 :
                    print "Channel %d:  Low TOT: %lf (NOT USABLE)" % (tChannel, ToT[1][tAsic][tChannel][i])
                    nNUToT+=1
                    continue_flag=1
                    break
            if(continue_flag):
                continue

          ###################################################
            for i in range(4):
                if TFineRMS[1][tAsic][tChannel][i]<1 and ToT[1][tAsic][tChannel][i]> 50 and ToT[1][tAsic][tChannel][i]< 80:
                    print "Channel %d:  Low TOT: %lf (MANUAL INSPECTION)" % (tChannel, ToT[1][tAsic][tChannel][i])
                    nMIToT+=1
                    continue_flag=1
                    break



        print "\nSummary:"
        if(args.mode!="fetp"):
            print "Dead on TDCA : %d" %  nDeadTAC
        if(args.mode!="tdca"):
            print "Dead on FETP : %d" %  nDeadFETP
        if(args.mode!="fetp"):
            print "Phase Problems on tFine on TDCA: %d" %  nTTDCMean
            print "Phase Problems on eFine on TDCA: %d" %  nETDCMean
            print "High tFine RMS TDCA: %d" %  nTTDCRms
            print "High eFine RMS TDCA: %d" %  nETDCRms
        if(args.mode!="tdca"):
            print "Phase Problems on tFine on FETP: %d" % nTFETPMean
            print "Phase Problems on eFine on FETP: %d" % nEFETPMean
            print "Manual inspection due to high tFine RMS: %d" %   nMIRms
            print "Not usable due to high tFine RMS: %d" %  nNURms 
            print "Manual inspection due to low ToT: %d" %  nMIToT 
            print "Not usable due to low ToT: %d" %   nNUToT
else:              
    if(flagBad==True):    
        print "\nRESULT: BAD ASIC!!!\n"
    else:    
        print "\nRESULT: GOOD ASIC!!!\n"



if(args.writeRoot):
    rootFile.Write()
    rootFile.Close()

log.close()

uut.setAllHVDAC(0)


exit()
