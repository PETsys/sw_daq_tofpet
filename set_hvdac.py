# -*- coding: utf-8 -*-

from sys import argv
import atb
from loadLocalConfig import loadLocalConfig

uut = atb.ATB("/tmp/d.sock")
uut.config = loadLocalConfig()
uut.setAllHVDAC(float(argv[1]))
