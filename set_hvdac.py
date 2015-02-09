# -*- coding: utf-8 -*-

from sys import argv
import atb
from loadLocalConfig import loadLocalConfig

uut = atb.ATB("/tmp/d.sock")
uut.config = loadLocalConfig()
for i in range(192,256):
	uut.setHVDAC(i, float(argv[1]))
#for i in range(192,256):
	#uut.setHVDAC_(i, float(35+i-192))
#for i in range(192,200):
	#uut.setHVDAC_(i, float(35))
#for i in range(200,208):
	#uut.setHVDAC_(i, float(40))
#for i in range(208,216):
	#uut.setHVDAC_(i, float(45))
#for i in range(216,224):
	#uut.setHVDAC_(i, float(50))
#for i in range(224,232):
	#uut.setHVDAC_(i, float(55))
#for i in range(232,240):
	#uut.setHVDAC_(i, float(60))
#for i in range(240,248):
	#uut.setHVDAC_(i, float(65))
#for i in range(248,256):
	#uut.setHVDAC_(i, float(70))

