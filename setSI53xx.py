# -*- coding: utf-8 -*-
import atb
from time import sleep
from sys import argv

uut = atb.ATB("/tmp/d.sock")
uut.config = atb.BoardConfig()
f = open(argv[1])

print "Configuring SI53xx clock filter"

for line in f:
	if line[0] == '#': continue
	line = line[:-1]
	regNum, regValue = line.split(', ')
	regNum = int(regNum)

	regValue = '0x' + regValue[:-1]
	regValue = int(regValue, base=16)

	print "Register %02x set to %02x" % (regNum, regValue)
	uut.setSI53xxRegister(regNum, regValue)

print "Done"
