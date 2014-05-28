# -*- coding: utf-8 -*-
# -*- coding: utf-8 -*-
import atb
from time import sleep

uut = atb.ATB("/tmp/d.sock")
#uut.initialize()
#uut.config = atb.BoardConfig()
#uut.uploadConfig()
##for c in range(8):
	##uut.config.hvBias[c] = 68.0
##for ac in uut.config.asicConfig:
	##ac.globalConfig.setValue("vib1", 32)

#uut.uploadConfig()

#uut.sendCommand(0x03, bytearray([0x00, 0x00, 0x00, 0xFF, 0xFF]))
uut.sendCommand(0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))





