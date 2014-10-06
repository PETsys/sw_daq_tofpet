# -*- coding: utf-8 -*-
# -*- coding: utf-8 -*-
import atb
from time import sleep

uut = atb.ATB("/tmp/d.sock")
uut.config = atb.BoardConfig();

while True:
	status, data0 = uut.doAsicCommand(0, "rdChDark", channel = 9)
	status, data1 = uut.doAsicCommand(1, "rdChDark", channel = 9)
	print data0, data1


	






