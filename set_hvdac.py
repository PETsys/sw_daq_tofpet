# -*- coding: utf-8 -*-

from sys import argv
import atb

uut = atb.ATB("/tmp/d.sock")
for i in range(32):
	uut.setHVDAC(i, float(argv[1]))

