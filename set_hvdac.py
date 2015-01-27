# -*- coding: utf-8 -*-

from sys import argv
import atb
from loadLocalConfig import loadLocalConfig

uut = atb.ATB("/tmp/d.sock")
uut.config = loadLocalConfig()
for i in range(192,256):
	uut.setHVDAC(i, float(argv[1]))

