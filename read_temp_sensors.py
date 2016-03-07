# -*- coding: utf-8 -*-
import atb
from time import sleep

uut = atb.ATB("/tmp/d.sock")
#while True:
for portID, slaveID in uut.getActiveFEBDs():
	try:
		nSensors = uut.getNumberOfTemperatureSensors(portID, slaveID)
		s = "FEB/D at port %d slave %d has %d sensor(s): " % (portID, slaveID, nSensors)
		t = uut.getTemperatureSensorReading(portID, slaveID, nSensors)
		s += (", ").join([str(x) + "ºC" for x in t])
		print s
	except Exception as e:
		print "FEB/D at port %d slave %d had an error: %s" % (portID, slaveID, e)