# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams
from sys import argv
import tofpet

AsicConfig = tofpet.AsicConfig
AsicGlobalConfig = tofpet.AsicGlobalConfig
AsicChannelConfig = tofpet.AsicChannelConfig
intToBin = tofpet.intToBin

# Create a default configuration for target ASICSs (2 per board)
atbConfig = atb.BoardConfig()
atbConfig.asicConfig[0]=AsicConfig()
atbConfig.asicConfig[1]=AsicConfig()
atbConfig.asicConfig[2]=AsicConfig()
atbConfig.asicConfig[3]=AsicConfig()


# Global input DC level adjustment
# Default is 48
atbConfig.asicConfig[0].globalConfig.setValue("sipm_idac_dcstart", 49)
atbConfig.asicConfig[1].globalConfig.setValue("sipm_idac_dcstart", 44)
atbConfig.asicConfig[2].globalConfig.setValue("sipm_idac_dcstart", 48)
atbConfig.asicConfig[3].globalConfig.setValue("sipm_idac_dcstart", 53)

# Channel input DC level adjustment
# Default is 44
for channelConfig in atbConfig.asicConfig[0].channelConfig:
	channelConfig.setValue("vbl", 26)
for channelConfig in atbConfig.asicConfig[1].channelConfig:
	channelConfig.setValue("vbl", 50)
for channelConfig in atbConfig.asicConfig[2].channelConfig:
	channelConfig.setValue("vbl", 32)
for channelConfig in atbConfig.asicConfig[3].channelConfig:
	channelConfig.setValue("vbl", 38)

# Global amplifier reference curerent ib1
# Default is 24
atbConfig.asicConfig[0].globalConfig.setValue("vib1", 10)
atbConfig.asicConfig[1].globalConfig.setValue("vib1", 10)
atbConfig.asicConfig[2].globalConfig.setValue("vib1", 10)
atbConfig.asicConfig[3].globalConfig.setValue("vib1", 5)


# Amplifier output DC level
# Default is 50
atbConfig.asicConfig[0].globalConfig.setValue("postamp", 42)
atbConfig.asicConfig[1].globalConfig.setValue("postamp", 45)
atbConfig.asicConfig[2].globalConfig.setValue("postamp", 43)
atbConfig.asicConfig[3].globalConfig.setValue("postamp", 43)

# Save the configuration into mezzanine/feba specific configuration files
#dumpAsicConfig(atbConfig, 0, 2, "config/M1/asic.config")
dumpAsicConfig(atbConfig, 0, 2, "config/FEBA0808/FEBA0207/asic.config")
dumpAsicConfig(atbConfig, 4, 6, "config/FEBA0808/FEBA0208/asic.config")

# Upload configuration into ATBConfig
uut = atb.ATB("/tmp/d.sock")
uut.config = atbConfig
uut.initialize()
uut.uploadConfig()



