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


parser.add_argument('OutputFilePrefix', help='Prefix for the output files.')

parser.add_argument('--hvBias', type=float, default=5.0, help='The voltage to be set for the HV DACs')

parser.add_argument('--tpDAC', type=int, default=32, help='The amplitude of the test pulse in DAC units (default: 32).')

parser.add_argument('--comments', type=str, default="", help='Any comments regarding the acquisition. These will be saved as a header in OutputFilePrefix.params')


args = parser.parse_args()


# Parameters
T = 6.25E-9
# PLL steps per clock
M = 392

phaseMin = 0.0
phaseMax = 8.0
phaseBins = 165

intervalMin = 16
intervalMax = 1016
intervalBins = 20

ipMin = 2.0
ipMax = 3.0
ipBins = 4


uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.config = loadLocalConfig(useBaseline=False)
uut.initialize()
uut.setAllHVDAC(args.hvBias)

f = open("%s.bins" % args.OutputFilePrefix, "w")
f.write("%d\t%f\t%f\n" % (phaseBins, phaseMin, phaseMax))
f.write("%d\t%f\t%f\n" % (intervalBins, intervalMin, intervalMax))
f.write("%d\t%f\t%f\n" % (ipBins, ipMin, ipMax))
f.close()

for pulse_type, scan_type in [ ("fetp", "linear"), ("fetp", "leakage"), ("tdca", "linear"), ("tdca", "leakage") ]:

	uut.config.cutToT = -25E-9
	uut.openAcquisition("%s_%s_%s" % (args.OutputFilePrefix, pulse_type, scan_type))
		
	if scan_type == "linear":
		phases = [ phaseMin + (b+0.5) * (phaseMax - phaseMin) / phaseBins for b in range(phaseBins) ]
		intervals = [ intervalMin ]
		events = 1000
	else:
		phases = [ ipMin + (b+0.5) * (ipMax-ipMin)/ipBins for b in range(ipBins) ]
		intervals = [ intervalMin + b * (intervalMax - intervalMin)/intervalBins for b in range(intervalBins) ]
		events = 300

	for channel in range(64):
		# Set ASIC config
		for ac in uut.config.asicConfig:
			if not ac: continue
			ac.globalConfig.setValue("test_pulse_en", 1)
			ac.globalTConfig = bitarray(atb.intToBin(args.tpDAC, 6) + '1')
			# Reset all channel to "normal" (non-TDCA) trigger and no-FETP
			for cc in ac.channelConfig:
				cc[52-47:52-42+1] = bitarray("000010")
			for n in range(64):
				ac.channelTConfig[n] = bitarray('0')

		
		# Set the channel to trigger
		# and select the TP polarity
		if pulse_type == "tdca":
			for ac in uut.config.asicConfig:
				if not ac: continue
				ac.channelConfig[channel][52-47:52-42+1] = bitarray("111111")

			pulseLow = False
		else:
			for ac in uut.config.asicConfig:
				if not ac: continue
				ac.channelTConfig[channel] = bitarray('1')

			pulseLow = True

		uut.uploadConfig()

		tpLength = 128
		for interval in intervals:
			uut.setTestPulsePLL(tpLength, interval, 0, pulseLow)
			acquisitionTime = 0.05 + events * (interval + 1) * (1024 * T)

			for phase in phases:
				print pulse_type, scan_type, channel, interval, phase
				tpPhase = int(round(phase * M))
				uut.setTestPulsePLL(tpLength, interval, tpPhase, pulseLow)
				uut.acquire(phase, interval+1, acquisitionTime)

	uut.closeAcquisition()
				
				

uut.setAllHVDAC(0)
