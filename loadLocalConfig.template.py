# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams, loadBaseline

# Loads configuration for the local setup
def loadLocalConfig(useBaseline=True):
	atbConfig = atb.BoardConfig()
	# HV DAC calibration
	loadHVDACParams(atbConfig, "config/pab1/hvdac.Config")

	### Mezzanine A (J15) configuration
	loadAsicConfig(atbConfig, 0, "config/M001/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 0, "config/M001/asic.baseline");

	### Mezzanine B (J16) configuration
	loadAsicConfig(atbConfig, 1, "config/M002/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 1, "config/M002/asic.baseline");


	return atbConfig
	
