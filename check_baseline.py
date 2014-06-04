import re
import numpy as np
from sys import argv
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

sipm_dc_0=int(argv[1])
sipm_dc_1=int(argv[2])

data0=readBaselineProposal("MA.baseline")
data1=readBaselineProposal("MB.baseline")

th1=data0[0]
sig1=data0[1]

th2=data1[0]
sig2=data1[1]


average_th1=np.average(th1)
average_th2=np.average(th2)
average_sig1=np.average(sig1)
average_sig2=np.average(sig2)

proposal0=0
proposal1=0

if (np.amin(th1)==0 or average_th1<10):
    #print "No counts were found for this postamp value: increasing 5 adc units"
    proposal0=-5
if (average_th1>62):
    proposal0=-2
if (average_th1>56 or np.amax(th1)>60 or np.amax(sig1) > 5):
    proposal0=-1
if (average_th1<49 or np.amin(th1)<46):
    proposal0=1
if (average_th1<46):
    proposal0=2
if (average_th1<40):
    proposal0=5

if (np.amin(th2)==0 or average_th2<10):
    #print "No counts were found for this postamp value: increasing 5 adc units"
    proposal1=-5
if (average_th2>62):
    proposal1=-2
if (average_th2>56 or np.amax(th2)>60 or np.amax(sig2) > 5):
    proposal1=-1
if (average_th2<49 or np.amin(th2)<46):
    proposal1=1
if (average_th2<46):
    proposal1=2
if (average_th2<40):
    proposal1=5

print sipm_dc_0+proposal0, sipm_dc_1+proposal1

if(proposal1==0 and np.std(th1)>1.5):
    print '\n'
    print "Warning: accepted baseline but the dispersion is significant"




#print np.average(th1),np.amin(th1), np.amax(th1), np.std(th1)
#print np.average(th2),np.amin(th2), np.amax(th2), np.std(th2)
#print np.average(sig1),np.amin(sig1), np.amax(sig1), np.std(sig1)
#print np.average(sig2),np.amin(sig2), np.amax(sig2), np.std(sig2)
    
    
