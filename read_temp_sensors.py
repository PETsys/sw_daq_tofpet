# -*- coding: utf-8 -*-
import atb
from time import sleep

uut = atb.ATB("/tmp/d.sock")
#while True:
sensorList = uut.getTemperatureSensorList()

for portID, slaveID, nSensors in sensorList:
	s = "FEB/D at port %d slave %d has %d sensor(s): " % (portID, slaveID, nSensors)
	t = uut.getTemperatureSensorReading(portID, slaveID, nSensors)
	s += (", ").join([str(x) + "ºC" for x in t])
	print s