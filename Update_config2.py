# -*- coding: utf-8 -*-
from sys import argv
import atb
from atbUtils import loadAsicConfig, dumpAsicConfig, loadHVDACParams


if(len(argv) is 1):
# Global input DC level adjustment
# Default is 48
	sipm_idac_dcstart_mez0=46
	sipm_idac_dcstart_mez1=52
else:
        sipm_idac_dcstart_mez0=int(argv[1])
        sipm_idac_dcstart_mez1=int(argv[2])

# Create a generic configuration
atbConfig = atb.BoardConfig()


# Global input DC level adjustment
# Default is 48
atbConfig.asicConfig[0].globalConfig.setValue("sipm_idac_dcstart",sipm_idac_dcstart_mez0 )
atbConfig.asicConfig[1].globalConfig.setValue("sipm_idac_dcstart",sipm_idac_dcstart_mez1 )

# Channel input DC level adjustment
# Default is 44
for channelConfig in atbConfig.asicConfig[0].channelConfig:
	channelConfig.setValue("vbl", 51)
for channelConfig in atbConfig.asicConfig[1].channelConfig:
	channelConfig.setValue("vbl", 39)


# Global amplifier reference curerent ib1
# Default is 24
atbConfig.asicConfig[0].globalConfig.setValue("vib1", 24)
atbConfig.asicConfig[1].globalConfig.setValue("vib1", 20)


# Discriminator DAC mininum
# Default is 55
atbConfig.asicConfig[0].globalConfig.setValue("disc_idac_dcstart", 55)
atbConfig.asicConfig[1].globalConfig.setValue("disc_idac_dcstart", 55)

# Amplifier output DC level
# Default is 50
atbConfig.asicConfig[0].globalConfig.setValue("postamp", 44)
atbConfig.asicConfig[1].globalConfig.setValue("postamp", 45)



# Save the configuration into mezzanine specific configuration files
#dumpAsicConfig(atbConfig, 0, "config/M021B/asic.config")
#dumpAsicConfig(atbConfig, 1, "config/M022B/asic.config")

# Upload configuration into ATBConfig
#uut = atb.ATB("/tmp/d.sock")
#uut.config = atbConfig
#uut.uploadConfig()



