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

parser.add_argument('--enableCoincidenceFilter', action='store_true', default=False, help='Enable the online coincidence filter. Default is off")')

parser.add_argument('--comments', type=str, default="", help='Any comments regarding the acquisition. These will be saved as a header in OutputFilePrefix.params')

args = parser.parse_args()



# ASIC clock period
T = 6.25E-9

acquisitionTime = args.acqTime
dataFilePrefix = args.OutputFilePrefix


uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.config = loadLocalConfig()
uut.initialize()
uut.openAcquisition(dataFilePrefix, args.enableCoincidenceFilter, writer="TOFPET")
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

