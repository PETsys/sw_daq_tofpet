# -*- coding: utf-8 -*-
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams
from sys import argv

# Create a generic configuration
atbConfig = atb.BoardConfig(nASIC=4)

# Global input DC level adjustment
# Default is 48
atbConfig.asicConfig[0].globalConfig.setValue("sipm_idac_dcstart", 48)
atbConfig.asicConfig[1].globalConfig.setValue("sipm_idac_dcstart", 48)
atbConfig.asicConfig[2].globalConfig.setValue("sipm_idac_dcstart", 48)
atbConfig.asicConfig[3].globalConfig.setValue("sipm_idac_dcstart", 48)

# Channel input DC level adjustment
# Default is 44
for channelConfig in atbConfig.asicConfig[0].channelConfig:
	channelConfig.setValue("vbl", 48)
for channelConfig in atbConfig.asicConfig[1].channelConfig:
	channelConfig.setValue("vbl", 48)
for channelConfig in atbConfig.asicConfig[2].channelConfig:
	channelConfig.setValue("vbl", 48)
for channelConfig in atbConfig.asicConfig[3].channelConfig:
	channelConfig.setValue("vbl", 48)

# Global amplifier reference curerent ib1
# Default is 24
atbConfig.asicConfig[0].globalConfig.setValue("vib1", 24)
atbConfig.asicConfig[1].globalConfig.setValue("vib1", 24)
atbConfig.asicConfig[2].globalConfig.setValue("vib1", 24)
atbConfig.asicConfig[3].globalConfig.setValue("vib1", 24)

# Discriminator DAC mininum
# Default is 55
atbConfig.asicConfig[0].globalConfig.setValue("disc_idac_dcstart", 55)
atbConfig.asicConfig[1].globalConfig.setValue("disc_idac_dcstart", 55)
atbConfig.asicConfig[2].globalConfig.setValue("disc_idac_dcstart", 55)
atbConfig.asicConfig[3].globalConfig.setValue("disc_idac_dcstart", 55)

# Amplifier output DC level
# Default is 50
atbConfig.asicConfig[0].globalConfig.setValue("postamp", 50)
atbConfig.asicConfig[1].globalConfig.setValue("postamp", 50)
atbConfig.asicConfig[2].globalConfig.setValue("postamp", 50)
atbConfig.asicConfig[3].globalConfig.setValue("postamp", 50)

# Save the configuration into mezzanine specific configuration files
dumpAsicConfig(atbConfig, 0, 2, "config/myBoard1/asic.config")
dumpAsicConfig(atbConfig, 2, 4, "config/myBoard2/asic.config")

# Upload configuration into ATBConfig
uut = atb.ATB("/tmp/d.sock")
uut.config = atbConfig
uut.uploadConfig()



