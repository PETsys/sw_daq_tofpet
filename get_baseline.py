# -*- coding: utf-8 -*-
import atb
import re
import os
import numpy as np
from loadLocalConfig import loadLocalConfig
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams
from sys import argv
from bitarray import bitarray
from sys import argv, stdout, stdin
from time import time, sleep
import ROOT
from os.path import join, dirname, basename, splitext
import tofpet


def readBaselineProposal(fileName):
    v=[]
    sig=[]
    data=[]
    #print "Reading %s" % (fileName)
    f = open(fileName, "r")
    r = re.compile('[ \t\n\r:]*')
    for i in range(64):
        l = f.readline()
        s =  r.split(l)
        s = s[0:4]
		#print s
        a, c, baseline, noise = s
        v.append(float(baseline))
        sig.append(float(noise))
    f.close()
    data.append(v)
    data.append(sig)
    return data

def dump_noise(root_file, uut, targetAsics, targetChannels):
  
    prefix, ext = splitext(root_file)
 
    # Only some channels
#targetChannels = [ (x, y) for x in targetAsics for y in [0, 2, 4, 6, 7, 9, 17, 23, 38, 49, 54, 63] ]

    # SiPM Vbias
    targetHVBias = [ 50 ]


    # Operating clock period
    T = 6.25E-9
    
    for tAsic, tChannel in targetChannels:
	atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("praedictio", 0)
   
    uut.uploadConfig()
 
    rootFile = ROOT.TFile(root_file, "RECREATE")
    ntuple = ROOT.TNtuple("data", "data", "step1:step2:asic:channel:rate")

    uut.config.writeParams(root_file)
    N = 30
    for step1 in targetHVBias:
        print "SiPM Vbias = ", step1

        for dacChannel in range(8):
            uut.setHVDAC(dacChannel, step1)

        for step2 in range(40,64):
            print "Vth_T = ", step2

            for tAsic, tChannel in [ (x, y) for x in targetAsics for y in range(64) ]:
                atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", step2)
                status, _ = uut.doTOFPETAsicCommand(tAsic, "wrChCfg", channel=tChannel, \
                                                      value=atbConfig.asicConfig[tAsic].channelConfig[tChannel])

		

            darkInterval = 0
            maxIntervalFound = dict([(ac, False) for ac in targetChannels])
            darkRate = dict([(ac, 0.0) for ac in targetChannels])

            while darkInterval < 16:
                for tAsic in targetAsics:
                    atbConfig.asicConfig[tAsic].globalConfig.setValue("count_intv", darkInterval)
                    status, _ = uut.doTOFPETAsicCommand(tAsic, "wrGlobalCfg", value=atbConfig.asicConfig[tAsic].globalConfig)
                    assert status == 0

                sleep(1024*(2**darkInterval) * T * 2)

                #print "Counting interval: %f ms" % (1024*(2**darkInterval) * T * 1E3)

                totalDarkCounts = dict([ (ac, 0) for ac in targetChannels ])
                maxDarkCounts = dict([ (ac, 0) for ac in targetChannels ])

                unfinishedChannels = [ ac for ac in targetChannels if maxIntervalFound[ac] == False ]
                for i in range(N):
                    for tAsic, tChannel in unfinishedChannels:	
                        status, data = uut.doTOFPETAsicCommand(tAsic, "rdChDark", channel=tChannel)
                        assert status == 0
                        v = atb.binToInt(data)
                        totalDarkCounts[(tAsic, tChannel)] += v
                        maxDarkCounts[(tAsic, tChannel)] = max([v, maxDarkCounts[(tAsic, tChannel)]])
					
				
                    sleep(1024*(2**darkInterval) * T)

                for ac in unfinishedChannels:
                    if maxDarkCounts[ac] > 512:
                        maxIntervalFound[ac] = True
                    else:
                        darkRate[ac] = float(totalDarkCounts[ac]) / (N * 1024*(2**darkInterval) * T)


                if False not in maxIntervalFound.values():
                    break;

                maxCount = max(maxDarkCounts.values())

                if maxCount == 0:
                    darkInterval += 4
                elif maxCount <= 32 and darkInterval < 11:
                    darkInterval += 4
                elif maxCount <= 64 and darkInterval < 12:
                    darkInterval += 3
                elif maxCount <= 128 and darkInterval < 13:
                    darkInterval += 3
                elif maxCount <= 256 and darkInterval < 14:
                    darkInterval += 2
                else:
                    darkInterval += 1
            if not any(x == 0 for x in darkRate.values()):
                print "Dark rate = ", darkRate.values()
            for tAsic, tChannel in targetChannels:
                ntuple.Fill(step1, step2, tAsic, tChannel, darkRate[(tAsic, tChannel)])
            rootFile.Write()

    rootFile.Close()


    for dacChannel in range(8):
	uut.setHVDAC(dacChannel, 0)

####################################################################################
#####################################################################################

post=[]
n_asics=4

if not (len(argv) >=3 and len(argv) <=8):
    print "USAGE: python %s root_file N_iterations [N_ASICS] [postamp0] [postamp1] ... [postampNASICS]" % argv[0]
    exit(1)

if (len(argv)  >= 4):
    n_asics= int(argv[3])

for i in range(n_asics):
    post.append(50)
    if (len(argv) >= 5+i):
        post[i] = int(argv[4+i]) 

        
T = 6.25E-9
root_file = argv[1]
n_iter=int(argv[2])

dir_path=os.path.dirname(root_file)
basename=os.path.basename(root_file)
os.system("mkdir -p %s/pdf" %  dir_path)

base_prefix, base_ext= splitext(basename)
prefix, ext = splitext(root_file)

log_filename= "%s.log" % prefix
log_f = open(log_filename, 'w+')

postamp=[0 for i in range(n_asics)]
success=[0 for i in range(n_asics)]


for i in range(n_asics):
    postamp[i]=post[i]
    success[i]=False

targetAsics = [ x for x in range(n_asics) ]
targetChannels = [ (x, y) for x in targetAsics for y in range(64) ]


i=1
finish=False

while i<=n_iter:

    print "\n#################### RUN %d ##########################" % i
    log_f.write( "\n#################### RUN %d ##########################\n" % i)

    atbConfig = loadLocalConfig(useBaseline=False)

    # Amplifier output DC level
    # Default is 50
    for asic in range(n_asics):
        atbConfig.asicConfig[asic].globalConfig.setValue("postamp", postamp[asic])
    

    # Upload configuration into ATBConfig

    uut = atb.ATB("/tmp/d.sock", False, F=1/T)
  
    uut.config = atbConfig
  
    uut.initialize()
  
    uut.uploadConfig()
 
    uut.doSync(False)
    
    root_filename= "%s%d%s" % (prefix,i,ext)


    print "\nPerforming threshold scan with:"
    for l in range(n_asics):
        print "postamp[%d]=%d" % (l,postamp[l])
    print "\n"

    check=dump_noise(root_filename,uut, targetAsics, targetChannels)
 #####################################################################3
    os.system("root -l %s \"draw_threshold_scan.C(true)\"" % root_filename)
    prefix, ext = splitext(root_file)
   
    pdf_filename= "%s/pdf/%s_%d.pdf" % (dir_path,base_prefix, i)
    os.system("cp /tmp/baseline_dummy.pdf %s" % pdf_filename)

    for j in range(n_asics):
        
        
        os.system("cp asic%d.baseline /tmp/asic%d_%d.baseline" % (j,j,i))
        data=readBaselineProposal("asic%d.baseline"% j)
            
        th=data[0]
        sig=data[1]
        average_th=np.average(th)
        average_sig=np.average(sig)
        nr_dead=0
            
        for value in th:
            if value==0:
                nr_dead+=1        




        if  (average_th==0):
     #print "No counts were found for this postamp value: increasing 5 adc units"
            proposal=-3
        elif ( (average_th>56 and (np.amax(th)-np.amin(th))< 9) or ( average_th>52 and (np.amax(th)-np.amin(th)) > 9)):
            proposal=-1
        elif (average_th>60 or nr_dead>6):
            proposal=-2
        elif ((average_th<47) or (np.amin(th)<45 and np.amin(th)!=0)):
            proposal=1
        elif (average_th<46):
            proposal=2
        elif (average_th<40):
            proposal=3
        else:
            proposal=0
                #targetAsics.pop([j])
                #targetChannels = [ (x, y) for x in targetAsics for y in range(64) ]
            success[j]=True

        print "\n\n############ ASIC %d: Postamp = %d ############" % (j, postamp[j])
        if success[j]:
            print "############  !!!!CONVERGED!!!!! ###########"
        print "Mean threshold: ", average_th, " +/- ", np.std(th)
        print "Mean threshold error: ", average_sig , " +/- ", np.std(sig)
        print "Min and Max threshold values: ", np.amin(th), np.amax(th)
        print "Min and Max threshold error values: ", np.amin(sig), np.amax(sig)
        print "Number of channels with no counts: ", nr_dead
        print "\n"

        log_f.write( "\n\n############ ASIC %d: Postamp = %d ############\n" % (j, postamp[j]))  
        if success[j]:
            log_f.write("############  !!!!CONVERGED!!!!! ###########\n")
        log_f.write( "Mean threshold: %f +/- %f\n" % (average_th,np.std(th)))
        log_f.write( "Mean threshold error: %f +/- %f \n" % (average_sig,np.std(sig)) )
        log_f.write( "Min and Max threshold values: %f  %f\n" % (np.amin(th), np.amax(th)))
        log_f.write( "Min and Max threshold error values: %f  %f\n" % (np.amin(sig), np.amax(sig)))
        log_f.write( "Number of channels with no counts: %ld" % nr_dead)
        log_f.write( "\n")
        log_f.flush()

        postamp[j]+=proposal
           


        if all(success): 
            os.system("evince %s &" % pdf_filename)
            print "\n\n Successfully converged for all asics after %d runs!!" % i
            print "Displaying baseline plots for: "
            for k in range (n_asics):
                print "postamp[%d]=%d" % (k,postamp[k])
            finish=True
            break
    
         
    if (i==n_iter):
        for j in range(n_asics):
            if success[j] is False: 
                print "Could not find a suitable baseline for ASIC/Mezzanine %d!" % j 
                print "Check log and plots in:"
                print "%s and %s/pdf" % (dir_path, log_filename)
                finish=True
    if finish:
        print "\n\n-------------- OPTIONS -------------------" 
        print "1 - Quit"
        print "2 - Save baseline files and Quit"
        print "3 - Refine one or more ASICS (with user defined postamp value(s))"
        opt=raw_input("Please choose an option:")
         
        option=int(opt)

        if (option != 1 and option != 2 and option != 3):
            print "Invalid option!"
            option=raw_input("Please choose a valid option:")
        elif(option==1):
            i=n_iter+1
            break
        elif(option==2):
            i=n_iter+1
            n_boards=n_asics/2;
            for j in range(n_boards):
                prefix, ext = splitext(uut.config.asicConfigFile[j*2])
                os.system("cat asic%d.baseline >> asic%d.baseline" % (2*j+1,2*j))
                os.system("cp asic%d.baseline %s.baseline" % (2*j,prefix))
            print "\nReminder: To complete process, please insert postamp values in update_config.py\n"
            break
        elif(option==3):
            n_refine=raw_input("Please enter the number of ASICS you need to refine:")
            print "Please enter the ID number and postamp value for the ASICS you want to refine:" 
            for k in range(int(n_refine)):
                id_number= raw_input("ID:")
                postamp[int(id_number)]=int(raw_input("Postamp[%d]:" % (int(id_number))))     
            i=n_iter
            continue

    i+=1
   

log_f.close()
#os.system("rm /tmp/*.baseline")
