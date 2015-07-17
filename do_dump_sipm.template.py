# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
import tofpet
import argparse


parser = argparse.ArgumentParser(description='Acquire data from SiPMs in all active boards')
parser.add_argument('acqTime', type=float,
                   help='acquisition time (in seconds)')

parser.add_argument('OutputFilePrefix',
                   help='output file prefix (files with .raw3 and .idx3 suffixes will be created)')

parser.add_argument('--cWindow', type=float, default=0, help='If set, defines the coincidence time window (in seconds) for preliminary selection of events (default is 0, accepting all events)')

parser.add_argument('--minToT', type=float, default=150, help='Sets the minimum ToT (in ns) for coincidence based preliminary selection of events (default is 150)')

parser.add_argument('--comments', type=str, default="", help='Any comments regarding the acquisition. These will be saved as a header in OutputFilePrefix.params')

args = parser.parse_args()



# ASIC clock period
T = 6.25E-9

if args.cWindow == None:
	cWindow = 0
else:
	cWindow = args.cWindow

if args.minToT == None:
	minTOT = 0
else:
	minToT = args.minToT
	
acquisitionTime = args.acqTime
dataFilePrefix = args.OutputFilePrefix


uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.config = loadLocalConfig()
uut.openAcquisition(dataFilePrefix, cWindow*1E-9, minToT*1E-9, writer="TOFPET")
uut.initialize()
# Set all HV DAC channels 
uut.setAllHVDAC(67.50)
uut.config.writeParams(dataFilePrefix, args.comments)

for step1 in [0]:
	for step2 in [0]:
		# Actually upload config into hardware
		uut.uploadConfig()
		uut.doSync()
		print "Acquiring step %d %d..." % (step1, step2)
		uut.acquire(step1, step2, acquisitionTime)
  

# Set SiPM bias to zero when finished!
uut.setAllHVDAC(0)

