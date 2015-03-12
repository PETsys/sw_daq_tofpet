# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams, loadBaseline, loadHVBias

# Loads configuration for the local setup
def loadLocalConfig(useBaseline=True):
	atbConfig = atb.BoardConfig(nASIC=4, nDAC=1)	# This sets the maximum number of ASIC and DAC in a config.

	# HV DAC calibration
	loadHVDACParams(atbConfig, 0, 32, "config/pab1/hvdac.Config")
	loadHVBias(atbConfig, 0, 32, "config/sipm_set1/hvbias.config")

	### Mezzanine A (J15) configuration
	loadAsicConfig(atbConfig, 0, 2, "config/FEBA01/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 0, 2, "config/FEBA01/asic.baseline");

	### Mezzanine B (J16) configuration
	loadAsicConfig(atbConfig, 2, 4, "config/FEBA02/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 2, 4, "config/FEBA02/asic.baseline");


	return atbConfig
	
