# -*- coding: utf-8 -*-
import atb
from loadLocalConfig import loadLocalConfig
from bitarray import bitarray
from sys import argv, stdout, stdin
from time import time, sleep
import ROOT

targetAsics = [ x for x in range(1) ]

# All channels
targetChannels = [ (x, y) for x in targetAsics for y in range(64) ]
# Only some channels
targetChannels = [ (x, y) for x in targetAsics for y in [8, 12, 20] ]


# SiPM Vbias
targetHVBias = [ 50 ]
targetHVBias = [ 50, 65, 65.5, 66, 66.5 ]



# Operating clock period
T = 6.25E-9

atbConfig = loadLocalConfig()
for tAsic, tChannel in targetChannels:
	atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("praedictio", 0)

uut = atb.ATB("/tmp/d.sock", False, F=1/T)
uut.initialize()
uut.config = atbConfig

rootFile = ROOT.TFile(argv[1], "RECREATE")
ntuple = ROOT.TNtuple("data", "data", "step1:step2:asic:channel:rate")

N = 30
for step1 in targetHVBias:
	print "SiPM Vbias = ", step1

	for c in range(8):
		atbConfig.hvBias[c] = step1

	uut.uploadConfig()
	uut.doSync(False)

	for step2 in range(32,64):
		print "Vth_T = ", step2

		for tAsic, tChannel in [ (x, y) for x in targetAsics for y in range(64) ]:
			atbConfig.asicConfig[tAsic].channelConfig[tChannel].setValue("vth_T", step2)
			status, _ = uut.doAsicCommand(tAsic, "wrChCfg", channel=tChannel, \
				value=atbConfig.asicConfig[tAsic].channelConfig[tChannel])

		

		darkInterval = 0
		maxIntervalFound = dict([(ac, False) for ac in targetChannels])
		darkRate = dict([(ac, 0.0) for ac in targetChannels])

		while darkInterval < 16:
			for tAsic in targetAsics:
				atbConfig.asicConfig[tAsic].globalConfig.setValue("count_intv", darkInterval)
				status, _ = uut.doAsicCommand(tAsic, "wrGlobalCfg", value=atbConfig.asicConfig[tAsic].globalConfig)
				assert status == 0

			sleep(1024*(2**darkInterval) * T * 2)

			print "Counting interval: %f ms" % (1024*(2**darkInterval) * T * 1E3)

			totalDarkCounts = dict([ (ac, 0) for ac in targetChannels ])
			maxDarkCounts = dict([ (ac, 0) for ac in targetChannels ])

			unfinishedChannels = [ ac for ac in targetChannels if maxIntervalFound[ac] == False ]
			for i in range(N):
				for tAsic, tChannel in unfinishedChannels:	
					status, data = uut.doAsicCommand(tAsic, "rdChDark", channel=tChannel)
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
