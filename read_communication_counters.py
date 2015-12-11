# -*- coding: utf-8 -*-
import atb
uut = atb.ATB("/tmp/d.sock")

print "--- DAQ PORTS ---" 
for portID in uut.getActivePorts():
	print "DAQ Port %2d: " % portID, "%12d transmitted %12d received (%d errors)" % uut.getPortCounts(portID)

print "--- FEB/Ds ---"

for portID, slaveID in uut.getActiveFEBDs():
	mtx, mrx, mrxBad, slaveOn, stx, srx, srxBad = uut.getFEBDCount1(portID, slaveID)
	print "FEB/D %2d slave %2d\n" % (portID, slaveID),
	print "  MASTER link %12d transmitted %12d received (%d errors)" % (mtx, mrx, mrxBad)
	if slaveOn:
		print "  SLAVE  link %12d transmitted %12d received (%d errors)" % (stx, srx, srxBad)
	else:
		print "  SLAVE  link off"
