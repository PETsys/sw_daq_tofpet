# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams, loadBaseline

# Loads configuration for the local setup
def loadLocalConfig(useBaseline=True):
	atbConfig = atb.BoardConfig(nASIC=4, nDAC=1))
	# HV DAC calibration
	loadHVDACParams(atbConfig, "config/pab1/hvdac.Config")

	### Mezzanine A (J15) configuration
	loadAsicConfig(atbConfig, 0, 2, "config/M001/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 0, 2, "config/M001/asic.baseline");

	### Mezzanine B (J16) configuration
	loadAsicConfig(atbConfig, 2, 4, "config/M002/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 2, 4, "config/M002/asic.baseline");


	return atbConfig
	
