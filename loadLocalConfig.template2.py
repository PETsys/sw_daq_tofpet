# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams, loadBaseline

# Loads configuration for the local setup
def loadLocalConfig(useBaseline=True):
	atbConfig = atb.BoardConfig()

        loadTriggerMap(atbConfig, "basic_channel.map", "basic_trigger.map")
	atbConfig.triggerMinimumToT = 150E-9
	atbConfig.triggerCoincidenceWindow = 25E-9

	# HV DAC calibration
	loadHVDACParams(atbConfig, "config/febd1/hvdac.Config")

	### FEBA (F1) configuration
	loadAsicConfig(atbConfig, 0, 2, "config/FEBA1/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 0, 2, "config/FEBA1/asic.baseline");

	### FEBA (F2) configuration
	loadAsicConfig(atbConfig, 2, 4, "config/FEBA2/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 2, 4, "config/FEBA2/asic.baseline");

	### FEBA (F3) configuration
	loadAsicConfig(atbConfig, 4, 6, "config/FEBA3/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 4, 6, "config/FEBA3/asic.baseline");

        ### FEBA (F4) configuration
	loadAsicConfig(atbConfig, 6, 8, "config/FEBA4/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 6, 8, "config/FEBA4/asic.baseline");

	### FEBA (F5) configuration
	loadAsicConfig(atbConfig, 8, 10, "config/FEBA5/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 8, 10, "config/FEBA5/asic.baseline");
	
        ### FEBA (F6) configuration
	loadAsicConfig(atbConfig, 10, 12, "config/FEBA6/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 10, 12, "config/FEBA6/asic.baseline");

	### FEBA (F7) configuration
	loadAsicConfig(atbConfig, 12, 14, "config/FEBA7/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 12, 14, "config/FEBA7/asic.baseline");

	### FEBA (F8) configuration
	loadAsicConfig(atbConfig, 14, 16, "config/FEBA8/asic.config")
	if useBaseline:
		loadBaseline(atbConfig, 14, 16, "config/FEBA8/asic.baseline");
		
	
	return atbConfig
	
