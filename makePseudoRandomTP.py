# -*- coding: utf-8 -*-
import random
import atb
from sys import argv
from math import ceil

targetRate = float(argv[1])
acqTime = float(argv[2])
dataFileName = argv[3]


uut = atb.ATB("/tmp/d.sock")
random.seed()



T = 6.25E-9/2

totalDelay = 0
lastLength = 0
n1 = 0
n2 = 0
pulseList = []
while n2 <2**16:
	if n1 % 4096 == 4095:
		print "%5d of %5d" % (n2+1, 2**16)
	delay = random.expovariate(targetRate)
	delayN = int(round(delay / T))
	#delayN = 16*2048
	delay = delayN * T
	length = 1024

	pulseList.append((delay*1E12, length * T*1E12))

	delayN -= lastLength # Discount length of previous pulse
	delayN -= 3 # Discount 3 due to state machine operation

	if delayN < 0:
		continue

	while delayN >= 2**21:
		print "Adding extra"
		# Use nopulses to increase pause time
		# Discount the 3+1 minimum clock delay
		uut.loadArbTestPulse(n2, False, 2**21-4, 0)		
		delayN -= 2**21
		n2 += 1
	
	uut.loadArbTestPulse(n2, True, delayN, length-1)		
	
	lastLength = length
	totalDelay += delay
	n1 += 1
	n2 += 1

print "Generated %d events with an average rate of %5.1f Hz" % (n1, 1/(totalDelay/n1))
print "Generator period is %f seconds" % totalDelay

nLoops = int(ceil(acqTime / totalDelay))
print "Writing out %d loops, worth %f seconds of data" % (nLoops, nLoops * totalDelay)

lDelay = 0L
f = open(dataFileName, "w")
for n in range(nLoops):
	for delay, length in pulseList:
		lDelay += long(delay)
		f.write("%ld %ld\n" % (lDelay, long(length)))
		
f.close()
	

