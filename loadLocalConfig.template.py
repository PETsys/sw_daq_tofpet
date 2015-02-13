# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams, loadBaseline, loadHVBias

# Loads configuration for the local setup
def loadLocalConfig(useBaseline=True):
	atbConfig = atb.BoardConfig(nASIC=64, nDAC=8)	# This sets the maximum number of ASIC and DAC in a config. No need to change.
	# HV DAC calibration
	loadHVDACParams(atbConfig, 0, 32, "config/pab1/hvdac.Config")
	loadHVBias(atbConfig, 0, 32, "config/sipm_set1/hvbias.config")

	### Mezzanine A (J15) configuration
	loadAsicConfig(atbConfig, 0, 2, "config/M001/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 0, 2, "config/M001/asic.baseline");

	### Mezzanine B (J16) configuration
	loadAsicConfig(atbConfig, 2, 4, "config/M002/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 2, 4, "config/M002/asic.baseline");


	return atbConfig
	
