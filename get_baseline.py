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
import argparse

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

def dump_noise(root_file, uut, activeAsics, targetChannels, targetHVBias):
  
    prefix, ext = splitext(root_file)

    # Operating clock period
    T = 6.25E-9
    
    for tAsic, tChannel in targetChannels:
	atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("praedictio", 0)
    uut.uploadConfig()
 
    rootFile = ROOT.TFile(root_file, "RECREATE")
    ntuple = ROOT.TNtuple("data", "data", "step1:step2:asic:channel:rate")

    uut.config.writeParams(root_file)
    N = 30
    
    step1=targetHVBias

 
    

    uut.setAllHVDAC(step1)
     
  
    for step2 in range(40,64): 
        print "Vth_T = ", step2

        for tAsic, tChannel in targetChannels:
            atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", step2)
            status, _ = uut.doTOFPETAsicCommand(tAsic, "wrChCfg", channel=tChannel, \
                                                      value=atbConfig.asicConfig[tAsic].channelConfig[tChannel])

		

        darkInterval = 0
        maxIntervalFound = dict([(ac, False) for ac in targetChannels])
        darkRate = dict([(ac, 0.0) for ac in targetChannels])

        while darkInterval < 16:
            for tAsic in activeAsics:
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
   
        print "Dark rate = ", darkRate.values()
        for tAsic, tChannel in targetChannels:
            ntuple.Fill(step1, step2, tAsic, tChannel, darkRate[(tAsic, tChannel)])
        rootFile.Write()

    rootFile.Close()


    for dacChannel in range(8):
	uut.setHVDAC(dacChannel, 0)

####################################################################################
#####################################################################################



parser = argparse.ArgumentParser(description='Performs a number of threshold dark counts scan and computes the effective baseline for each, while ajusting the postamp parameter until the obtained baseline for all channels is in a good ADC range for the all the selected ASICS')

parser.add_argument('OutputFile',
                   help='output file (ROOT file). Auxiliary file containing the data obtained in all scans and used to compute the baselines')


parser.add_argument('nIter', type=int,
                   help='Maximum number of iterations to determine baseline')

parser.add_argument('hvBias', type=float,
                   help='The voltage to be set for the HV DACs')

parser.add_argument('--asics', nargs='*', type=int, help='If set, only the selected asics will acquire data')

parser.add_argument('--postamp', nargs='*', type=int, help='If set, this argument takes the postamp values from which to start the initial scan (the values should be ordered per asic ID)')

args = parser.parse_args()

        
T = 6.25E-9
root_file = args.OutputFile
n_iter=args.nIter

dir_path=os.path.dirname(root_file)
basename=os.path.basename(root_file)
os.system("mkdir -p %s/pdf" %  dir_path)

base_prefix, base_ext= splitext(basename)
prefix, ext = splitext(root_file)

log_filename= "%s.log" % prefix
log_f = open(log_filename, 'w+')

atbConfig = loadLocalConfig(useBaseline=False)  
uut = atb.ATB("/tmp/d.sock", False, F=1/T) 
uut.config = atbConfig
uut.initialize()

if args.asics == None:
	activeAsics =  uut.getActiveTOFPETAsics()
else:
	activeAsics= args.asics

maxAsics=max(activeAsics) + 1
systemAsics = [ i for i in range(maxAsics) ]

targetChannels = [ (x, y) for x in activeAsics for y in range(64) ]

postamp=[0 for i in range(maxAsics)]
success=[0 for i in range(maxAsics)]



for i,asic in enumerate(activeAsics):
    postamp[asic]=50
    if (args.postamp!= None):
        postamp[asic] = args.postamp[i]
    success[asic]=False
 
i=1
finish=False

while i<=n_iter:

    print "\n#################### RUN %d ##########################" % i
    log_f.write( "\n#################### RUN %d ##########################\n" % i)

    atbConfig = loadLocalConfig(useBaseline=False)
   

    uut = atb.ATB("/tmp/d.sock", False, F=1/T)
  
    uut.config = atbConfig
  
    uut.initialize()
  
    for asic in activeAsics:
        atbConfig.asicConfig[asic].globalConfig.setValue("postamp", postamp[asic])
    

    uut.uploadConfig()
 
    uut.doSync(False)
    
    root_filename= "%s%d%s" % (prefix,i,ext)


    print "\nPerforming threshold scan with:"
    for  asic in activeAsics:
        print "postamp[%d]=%d" % (asic,postamp[asic])
    print "\n"

    check=dump_noise(root_filename,uut, activeAsics, targetChannels, args.hvBias)
 
    os.system("root -l %s \"draw_threshold_scan.C(%s,true)\"" % (root_filename,maxAsics))
    prefix, ext = splitext(root_file)
   
    pdf_filename= "%s/pdf/%s_%d.pdf" % (dir_path,base_prefix, i)
    os.system("cp /tmp/baseline_dummy.pdf %s" % pdf_filename)

    for j in activeAsics:
        
        
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
           
        
        for j in activeAsics:
            if success[j] is True:
                Converged=True
            else:
                Converged=False
                break
        if(i==n_iter):
            for j in activeAsics:
                if success[j] is False:
                    print "Could not find a suitable baseline for ASIC/Mezzanine %d!" % j 
                    print "Check log and plots in:"
                    print "%s and %s/pdf" % (dir_path, log_filename)
                    Converged=False
                    finish=True
            print "Check log and plots in:"
            print "%s and %s/pdf" % (dir_path, log_filename)   
                
        if Converged: 
            os.system("evince %s &" % pdf_filename)
            print "\n\nSuccessfully converged for all asics after %d run(s)!!" % i
            print "Displaying baseline plots for: "
            for k in activeAsics:
                print "postamp[%d]=%d" % (k,postamp[k])
            finish=True
            break
                
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
            for asic in activeAsics:
            
                prefix, ext = splitext(uut.config.asicConfigFile[asic])
                if asic%2==0:
                    os.system("cat asic%d.baseline >> asic%d.baseline" % (asic+1,asic))
                    os.system("cp asic%d.baseline %s.baseline" % (asic,prefix))
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
   
os.system("rm *.baseline")
log_f.close()

