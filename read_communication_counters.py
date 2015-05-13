# -*- coding: utf-8 -*-
import atb
uut = atb.ATB("/tmp/d.sock")

print "DAQ ports"
for portID in uut.getActivePorts():
	print "Port %2d: " % portID, "TX %8d RX %8d RX error %8d" % uut.getPortCounts(portID)

print "FEB/D"
for portID, slaveID in uut.getActiveFEBDs():
	print "Port %2d slave %2d " % (portID, slaveID), "TX %8d RX %8d RX error %8d" % uut.getFEBDCount1(portID, slaveID)