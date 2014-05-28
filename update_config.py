# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams
from sys import argv

# Create a generic configuration
atbConfig = atb.BoardConfig()


# Global input DC level adjustment
# Default is 48
atbConfig.asicConfig[0].globalConfig.setValue("sipm_idac_dcstart", 48)
atbConfig.asicConfig[1].globalConfig.setValue("sipm_idac_dcstart", 46)

# Channel input DC level adjustment
# Default is 44
for channelConfig in atbConfig.asicConfig[0].channelConfig:
	channelConfig.setValue("vbl", 20)
for channelConfig in atbConfig.asicConfig[1].channelConfig:
	channelConfig.setValue("vbl", 20)


# Global amplifier reference curerent ib1
# Default is 24
atbConfig.asicConfig[0].globalConfig.setValue("vib1", 25)
atbConfig.asicConfig[1].globalConfig.setValue("vib1", 25)


# Discriminator DAC mininum
# Default is 55
atbConfig.asicConfig[0].globalConfig.setValue("disc_idac_dcstart", 55)
atbConfig.asicConfig[1].globalConfig.setValue("disc_idac_dcstart", 55)

# Amplifier output DC level
# Default is 50
atbConfig.asicConfig[0].globalConfig.setValue("postamp", 40)
atbConfig.asicConfig[1].globalConfig.setValue("postamp", 50)



# Save the configuration into mezzanine specific configuration files
dumpAsicConfig(atbConfig, 0, "config/M017/asic.config")
dumpAsicConfig(atbConfig, 1, "config/M018/asic.config")

# Upload configuration into ATBConfig
uut = atb.ATB("/tmp/d.sock")
uut.config = atbConfig
uut.uploadConfig()



