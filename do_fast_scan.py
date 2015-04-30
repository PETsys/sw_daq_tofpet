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
# Parameters
T = 6.25E-9
nominal_m = 128

#### PLL based generator
Generator = 1
M = 0x348	# 80 MHz PLL, 80 MHz AIC
M = 392		# 160 MHz PLL, 160 MHz ASIC
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



Nmax = 8
tpLength = 128
tpFrameInterval = 16
tpCoarsePhase = 1
tpFinePhase = 196


#rootFileName = argv[1] ##########



uut = atb.ATB("/tmp/d.sock", False, F=1/T)
atbConfig = loadLocalConfig(useBaseline=False)
defaultConfig=atbConfig
uut.config=atbConfig
uut.initialize()

rootFileName="/tmp/hists.root"
rootFile = ROOT.TFile(rootFileName, "RECREATE");
#rootData1 = DataFile( rootFile, "3")
#rootData2 = DataFile( rootFile, "3B")

activeChannels = [ x for x in range(0,64) ]
activeAsics =  [0,1]#[ i for i,ac in enumerate(uut.config.asicConfig) ]
activeAsics =  uut.getActiveTOFPETAsics()


for tAsic in activeAsics:
    print "-------------------"
    print "Config for ASIC ", tAsic
    print "dc_start= ",atbConfig.asicConfig[tAsic].globalConfig.getValue("sipm_idac_dcstart")
    print "vbl= ",atbConfig.asicConfig[tAsic].channelConfig[0].getValue("vbl")
    print "ib1= ",atbConfig.asicConfig[tAsic].globalConfig.getValue("vib1")
    print "postamp= ", atbConfig.asicConfig[tAsic].globalConfig.getValue("postamp")

print "-------------------\n"
maxAsics=max(activeAsics) + 1
systemAsics = [ i for i in range(maxAsics) ]

minEventsA *= len(activeAsics)
minEventsB *= len(activeAsics)

s=(2,maxAsics,64,4)


deadChannels=np.zeros(s)
nlargeTFine=np.zeros((2,maxAsics))
largeTFineTAC=np.zeros((2,maxAsics,64,4))
largeTFineRMS=np.zeros((2,maxAsics,64,4))
nsmallToT=np.zeros((2,maxAsics))
smallToTTAC=np.zeros((2,maxAsics,64,4))
smallToT=np.zeros((2,maxAsics,64,4))

TFineRMS=np.zeros((2,maxAsics,64,4))
ToT=np.zeros((2,maxAsics,64,4))


#print activeAsics, minEventsA, minEventsB
#hTPoint = ROOT.TH1F("hTPoint", "hTPoint", 64, 0, 64)

hTFine = [[[[ ROOT.TH1F("htFine_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "T Fine", 1024, 0, 1024) for finephase in range(4)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]
hEFine = [[[[ ROOT.TH1F("heFine_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "E Fine", 1024, 0, 1014) for finephase in range(4)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]
hToT = [[[[ ROOT.TH1F("heToT_%03d_%02d_%1d_%1d" % (tAsic, tChannel, tac, finephase), "Coarse ToT", 1024, 0, 1014) for finephase in range(4)]for tac in range(4) ] for tChannel in range(64)] for tAsic in systemAsics ]


for tdcaMode in [True,False]:
    if tdcaMode:
        mode=0
    else:
        mode=1
    for  asic in range(len(activeAsics)):
        print asic
        for  channel in range(64):
            for  tac in range(4):
                for  finephase in range(4):
                    hTFine[asic][channel][tac][finephase].Reset()
                    hEFine[asic][channel][tac][finephase].Reset()
                    hToT[asic][channel][tac][finephase].Reset()


    for i in range(4):
        tpFinePhase=i*98+1
        
        nEvents=0
        nDead=0
        #nlargeTFine=0
        #nlargeEFine=0
        #nsmallToT=0
       # deadChannels=[]
       # largeTFineChannel=[]
       # largeTFineTAC=[]
       # largeTFineRMS=[]
       # largeEFineChannel=[]
       # largeEFineTAC=[]
       # largeEFineRMS=[]
        nManualtfine=np.zeros(maxAsics)
        nNUtfine=np.zeros(maxAsics)
        nManualTOT=np.zeros(maxAsics) 
        nNUTOT=np.zeros(maxAsics)

      #  smallToTChannel=[]
      #  smallToTTAC=[]
       # smallToT=[]

        if tdcaMode:
            vbias = 5
            pulseLow = False
            frameInterval = 0
        else:
            frameInterval = 16
            tpDAC = 32
            vbias = 5
            pulseLow = True

        if vbias > 50: minEventsA *= 10

        for tChannel in activeChannels:

            if(tdcaMode):
                print "Running for Channel %d; Test pulse fine phase %d; mode TDCA" %(tChannel, tpFinePhase)
            else:
                print "Running for Channel %d; Test pulse fine phase %d; mode FETP" %(tChannel, tpFinePhase)
            
            atbConfig = loadLocalConfig(useBaseline=False)
            for c in range(len(atbConfig.hvBias)):
                atbConfig.hvBias[c] = vbias

            for tAsic in activeAsics:
                atbConfig.asicConfig[tAsic].globalConfig.setValue("test_pulse_en", 1)

            for tAsic in activeAsics:

                atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("fe_test_mode", 1)
                if tdcaMode:

                    # Both TDC branches
                    atbConfig.asicConfig[tAsic].channelConfig[tChannel][52-47:52-42+1] = bitarray("11" + "11" + "1" + "1") 


                else:				
                    atbConfig.asicConfig[tAsic].channelTConfig[tChannel] = bitarray('1')
                    atbConfig.asicConfig[tAsic].globalTConfig = bitarray(atb.intToBin(tpDAC, 6) + '1')


            uut.stop()
            uut.config = atbConfig
            uut.uploadConfig()
            uut.start()
            uut.setTestPulsePLL(tpLength, tpFrameInterval, tpFinePhase, pulseLow)
            uut.doSync()

        
            t0 = time()


            #print "check1", tChannel, nReceivedEvents
           
            nReceivedEvents = 0
            nAcceptedEvents = 0
            nReceivedFrames = 0
            #print "check2", tChannel, nReceivedEvents

            t0 = time()        
            while nAcceptedEvents < minEventsA and (time() - t0) < 10:

                decodedFrame = uut.getDataFrame(nonEmpty=True)
                if decodedFrame is None: continue

                nReceivedFrames += 1

                for asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime in decodedFrame['events']:
                    nReceivedEvents += 1

                    #if asic not in activeAsics or channel != tChannel:
                    #   continue;
                    #    if tdcaMode==False:
                    #       print asic, tac, tCoarse, eCoarse, eCoarse-tCoarse,  6.25*(eCoarse-tCoarse)
                    nAcceptedEvents += 1				
                    nEvents += 1	
                    #rootData1.addEvent(step1, step2, decodedFrame['id'], asic, channel, tac, tCoarse, eCoarse, tFine, eFine, channelIdleTime, tacIdleTime)

                    hTFine[asic][channel][tac][i].Fill(tFine)
                    hEFine[asic][channel][tac][i].Fill(eFine)
                    hToT[asic][channel][tac][i].Fill((eCoarse-tCoarse)*6.25)

         
            print "Channel %(tChannel)d: Got %(nReceivedEvents)d events in  %(nReceivedFrames)d frames, accepted %(nAcceptedEvents)d" % locals()
          


    avTRMS=0
    avERMS=0

   
    for tChannel in activeChannels:   
        for tAsic in activeAsics:
            tfineflag=True
            efineflag=True
            totflag=True
            for tac in range(4):
                if (hTFine[tAsic][tChannel][tac][0].GetEntries()==0 and hTFine[tAsic][tChannel][tac][1].GetEntries() == 0 and hTFine[tAsic][tChannel][tac][2].GetEntries()==0 and hTFine[tAsic][tChannel][tac][3].GetEntries() == 0):
                    deadChannels[mode][tAsic][tChannel][tac]=1
                    
                tRMS=min(hTFine[tAsic][tChannel][tac][0].GetRMS(), hTFine[tAsic][tChannel][tac][1].GetRMS(), hTFine[tAsic][tChannel][tac][2].GetRMS(), hTFine[tAsic][tChannel][tac][3].GetRMS() ) 
                tMean=min(hTFine[tAsic][tChannel][tac][0].GetMean(), hTFine[tAsic][tChannel][tac][1].GetMean(), hTFine[tAsic][tChannel][tac][2].GetMean(), hTFine[tAsic][tChannel][tac][3].GetMean() ) 
               # eRMS=min(hEFine[tAsic][tChannel][tac][0].GetRMS(), hEFine[tAsic][tChannel][tac][1].GetRMS(), hEFine[tAsic][tChannel][tac][2].GetRMS(), hEFine[tAsic][tChannel][tac][3].GetRMS())
                coarseTOT=max(hToT[tAsic][tChannel][tac][0].GetMean(), hToT[tAsic][tChannel][tac][1].GetMean(), hToT[tAsic][tChannel][tac][2].GetMean(), hToT[tAsic][tChannel][tac][3].GetMean())
                
             
                #print tAsic, tChannel, tac, tRMS, eRMS, hTFine[tAsic][tChannel][tac].Integral()
               # if (tRMS > 1.1 and tfineflag):
               #     nlargeTFine[mode][tAsic]+=1
               #     largeTFineTAC[mode][tAsic][tChannel][tac]=1
                TFineRMS[mode][tAsic][tChannel][tac]=tRMS
               #     tfineflag=False

                #avERMS+=eRMS
                #if (eRMS > 1 and efineflag):
                #    nlargeEFine[mode][tAsic]+=1
                #    largeEFineChannel[mode][tAsic][tChannel]=1
                #    largeEFineRMS[mode][tAsic][tChannel]=eRMS
                #    efineflag=False
                
                
                #nsmallToT[mode][tAsic]+=1
                #smallToTTAC[mode][tAsic][tChannel][tac]=1
                ToT[mode][tAsic][tChannel][tac]=coarseTOT
                #    totflag=False 
                    
   

for tAsic in activeAsics:
    nDeadTAC=0
    nLargeRmsTAC=0
    nDeadFETP=0
    nMIRms=0
    nMIToT=0
    nNURms=0
    nNUToT=0
    print "\n\n############ Report for ASIC %d ############\n\n" % tAsic
    for tChannel in activeChannels:
        #if any(deadChannels[0][tAsic][tChannel][:])==1:
        #    print "Channel %d: DEAD on TDCA" % tChannel
        #    nDeadTAC+=1
        #    continue
        #if any(largeTFineTAC[0][tAsic][tChannel][:])==1:
        #    print "Channel %d: High RMS on TDCA" % tChannel
        #    nLargeRmsTAC+=1
        #    continue
        if any(deadChannels[1][tAsic][tChannel][:])==1:
            print "Channel %d:  DEAD on FETP" % tChannel
            nDeadFETP+=1
            continue
 ###########################################################       
        if (TFineRMS[1][tAsic][tChannel][0]> 1 and TFineRMS[1][tAsic][tChannel][0]<2 and ToT[1][tAsic][tChannel][0]> 50 and ToT[1][tAsic][tChannel][0]<80):
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][0], ToT[1][tAsic][tChannel][0])
            nMIRms+=1
            nMIToT+=1
            continue
        
        if (TFineRMS[1][tAsic][tChannel][1]> 1 and TFineRMS[1][tAsic][tChannel][1]<2 and ToT[1][tAsic][tChannel][1]> 50 and ToT[1][tAsic][tChannel][1]<80):
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][1], ToT[1][tAsic][tChannel][1])
            nMIRms+=1
            nMIToT+=1
            continue

        if (TFineRMS[1][tAsic][tChannel][2]> 1 and TFineRMS[1][tAsic][tChannel][2]<2 and ToT[1][tAsic][tChannel][2]> 50 and ToT[1][tAsic][tChannel][2]<80):
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][2], ToT[1][tAsic][tChannel][2])
            nMIRms+=1
            nMIToT+=1
            continue

        if (TFineRMS[1][tAsic][tChannel][3]> 1 and TFineRMS[1][tAsic][tChannel][3]<2 and ToT[1][tAsic][tChannel][3]> 50 and ToT[1][tAsic][tChannel][3]<80):
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][3], ToT[1][tAsic][tChannel][3])
            nMIRms+=1
            nMIToT+=1
            continue

#######################################################

        if TFineRMS[1][tAsic][tChannel][0]> 1 and TFineRMS[1][tAsic][tChannel][0]<2 and ToT[1][tAsic][tChannel][0]<50:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][0], ToT[1][tAsic][tChannel][0])
            nMIRms+=1
            nNUToT+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][1]> 1 and TFineRMS[1][tAsic][tChannel][1]<2 and ToT[1][tAsic][tChannel][1]<50:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][1], ToT[1][tAsic][tChannel][1])
            nMIRms+=1
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]> 1 and TFineRMS[1][tAsic][tChannel][2]<2 and ToT[1][tAsic][tChannel][2]<50:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][2], ToT[1][tAsic][tChannel][2])
            nMIRms+=1
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]> 1 and TFineRMS[1][tAsic][tChannel][3]<2 and ToT[1][tAsic][tChannel][3]<50:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][3], ToT[1][tAsic][tChannel][3])
            nMIRms+=1
            nNUToT+=1
            continue
          
######################################################

        if TFineRMS[1][tAsic][tChannel][0]>2 and ToT[1][tAsic][tChannel][0]> 50 and ToT[1][tAsic][tChannel][0]<80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][0], ToT[1][tAsic][tChannel][0])
            nNURms+=1
            nMIToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][1]>2 and ToT[1][tAsic][tChannel][1]> 50 and ToT[1][tAsic][tChannel][1]<80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][1], ToT[1][tAsic][tChannel][1])
            nNURms+=1
            nMIToT+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][2]>2 and ToT[1][tAsic][tChannel][2]> 50 and ToT[1][tAsic][tChannel][2]<80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][2], ToT[1][tAsic][tChannel][2])
            nNURms+=1
            nMIToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]>2 and ToT[1][tAsic][tChannel][3]> 50 and ToT[1][tAsic][tChannel][3]<80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][3], ToT[1][tAsic][tChannel][3])
            nNURms+=1
            nMIToT+=1
            continue

###################################################
        if TFineRMS[1][tAsic][tChannel][0]>2 and ToT[1][tAsic][tChannel][0]< 50:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][0], ToT[1][tAsic][tChannel][0])
            nNURms+=1
            nNUToT+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][1]>2 and ToT[1][tAsic][tChannel][1]< 50:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][1], ToT[1][tAsic][tChannel][1])
            nNURms+=1
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]>2 and ToT[1][tAsic][tChannel][2]< 50:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][2], ToT[1][tAsic][tChannel][2])
            nNURms+=1
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]>2 and ToT[1][tAsic][tChannel][3]< 50:
            print "Channel %d:  High RMS: %lf (NOT USABLE)  and Low TOT: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][3], ToT[1][tAsic][tChannel][3])
            nNURms+=1
            nNUToT+=1
            continue

###################################################
        if TFineRMS[1][tAsic][tChannel][0]>2 and ToT[1][tAsic][tChannel][0]>80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][0])
            nNURms+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][1]>2 and ToT[1][tAsic][tChannel][1]>80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][1])
            nNURms+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]>2 and ToT[1][tAsic][tChannel][2]>80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][2])
            nNURms+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]>2 and ToT[1][tAsic][tChannel][3]>80:
            print "Channel %d:  High RMS: %lf (NOT USABLE)" % (tChannel,TFineRMS[1][tAsic][tChannel][3])
            nNURms+=1
            continue
        
###################################################
        if TFineRMS[1][tAsic][tChannel][0]>1 and TFineRMS[1][tAsic][tChannel][0]<2 and ToT[1][tAsic][tChannel][0]>80:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][0])
            nMIRms+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][1]>1 and TFineRMS[1][tAsic][tChannel][1]<2 and ToT[1][tAsic][tChannel][1]>80:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][1])
            nMIRms+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]>1 and TFineRMS[1][tAsic][tChannel][2]<2 and ToT[1][tAsic][tChannel][2]>80:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][2])
            nMIRms+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]>1 and TFineRMS[1][tAsic][tChannel][3]<2 and ToT[1][tAsic][tChannel][3]>80:
            print "Channel %d:  High RMS: %lf (MANUAL INSPECTION)" % (tChannel,TFineRMS[1][tAsic][tChannel][3])
            nMIRms+=1
            continue


###################################################
        
        if TFineRMS[1][tAsic][tChannel][0]<1 and ToT[1][tAsic][tChannel][0]< 50 :
            print "Channel %d:  Low TOT: %lf (NOT USABLE)" % (tChannel, ToT[1][tAsic][tChannel][0])
            nNUToT+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][1]<1 and ToT[1][tAsic][tChannel][1]< 50:
            print "Channel %d:  Low TOT: %lf (NOT USABLE)" % (tChannel, ToT[1][tAsic][tChannel][1])
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]<1 and ToT[1][tAsic][tChannel][2]< 50:
            print "Channel %d:  Low TOT: %lf (NOT USABLE)" % (tChannel, ToT[1][tAsic][tChannel][2])
            nNUToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][3]<1 and ToT[1][tAsic][tChannel][3]< 50:
            print "Channel %d:  Low TOT: %lf (NOT USABLE)" % (tChannel,ToT[1][tAsic][tChannel][3])
            nNUToT+=1
            continue

###################################################
        
        if TFineRMS[1][tAsic][tChannel][0]<1 and ToT[1][tAsic][tChannel][0]> 50 and ToT[1][tAsic][tChannel][0]< 80:
            print "Channel %d:  Low TOT: %lf (MANUAL INSPECTION)" % (tChannel, ToT[1][tAsic][tChannel][0])
            nMIToT+=1
            continue
        if TFineRMS[1][tAsic][tChannel][1]<1 and ToT[1][tAsic][tChannel][1]> 50 and ToT[1][tAsic][tChannel][1]< 80:
            print "Channel %d:  Low TOT: %lf (MANUAL INSPECTION)" % (tChannel, ToT[1][tAsic][tChannel][1])
            nMIToT+=1
            continue

        if TFineRMS[1][tAsic][tChannel][2]<1 and ToT[1][tAsic][tChannel][2]> 50 and ToT[1][tAsic][tChannel][2]< 80:
            print "Channel %d:  Low TOT: %lf (MANUAL INSPECTION)" % (tChannel, ToT[1][tAsic][tChannel][2])
            nMIToT+=1
            continue
        
        if TFineRMS[1][tAsic][tChannel][3]<1 and ToT[1][tAsic][tChannel][3]> 50 and ToT[1][tAsic][tChannel][3]< 80:
            print "Channel %d:  Low TOT: %lf (MANUAL INSPECTION)" % (tChannel, ToT[1][tAsic][tChannel][3])
            nNUToT+=1
            continue

       


    print "\nSummary\n:"
    print "Dead on TDC : %d\n" %  nDeadTAC
    print "Dead on FETP : %d\n" %  nDeadFETP
    print "High RMS on TDC: %d\n" %  nLargeRmsTAC
    print "Manual inspection due to high RMS: %d\n" %   nMIRms
    print "Not usable due to high RMS: %d\n" %  nNURms 
    print "Manual inspection due to low ToT: %d\n" %  nMIToT 
    print "Not usable due to low ToT: %d\n" %   nNUToT


rootFile.Write()
rootFile.Close()

uut.setAllHVDAC(0)
