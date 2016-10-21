# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
import tofpet
import argparse
from bitarray import bitarray
import time

parser = argparse.ArgumentParser(description='Acquire data from SiPMs while activating LED pulses with FPGA test pulse generator')
parser.add_argument('acqTime', type=float,
                   help='acquisition time (in seconds)')

parser.add_argument('OutputFilePrefix',
                   help='output file prefix (files with .raw3 and .idx3 suffixes will be created)')


parser.add_argument('--comments', type=str, default="", help='Any comments regarding the acquisition. These will be saved as a header in OutputFilePrefix.params')

args = parser.parse_args()



# ASIC clock period
T = 6.25E-9

	
acquisitionTime = args.acqTime
dataFilePrefix = args.OutputFilePrefix

tpFrameInterval = 0
tpFinePhase = 0
tpLength =  2
tpInvert = True # LED turns on when pulse is low

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.config = loadLocalConfig()

uut.initialize()
uut.openAcquisition(dataFilePrefix, False, writer="TOFPET")

# Enables TAC refresh
# Slightly worse time resolution at rates > 100 Hz /channelConfig
# Needed for rates < 100 Hz/channel
uut.setTacRefresh(True)

for step1 in [0]:
	tpFinePhase = step1 
	for tChannel in range(64):
		for asic in range(2):
			uut.config.asicConfig[asic].channelConfig[tChannel].setValue("vth_T", 10)
			uut.config.asicConfig[asic].channelConfig[tChannel].setValue("vth_E", 10)

	for step2 in range(0,7): # Scan frequencies from 156 kHz down to 20 kHz
                tpFrameInterval = step2

		uut.setAllHVDAC(27.5)
		uut.setTestPulsePLL(tpLength, tpFrameInterval, tpFinePhase, tpInvert, singleShot=False)
			
		uut.config.writeParams(dataFilePrefix, args.comments)
		uut.uploadConfig()
		uut.doSync()		
		print "Acquiring step %f %f..." % (step1, step2)
		# Adjust acquisition time to pulse repetition rate to keep number of events constant
		uut.acquire(step1,step2, acquisitionTime * (tpFrameInterval+1))

# Set SiPM bias to zero when finished!
uut.setAllHVDAC(0)
uut.setTestPulseNone()

