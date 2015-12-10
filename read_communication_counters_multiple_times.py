# -*- coding: utf-8 -*-
import atb
from datetime import datetime


uut = atb.ATB("/tmp/d.sock")

# open file for logging, line buffered
ofp = open("communication_error_counters.log",'a',1)
ofp.writelines(['\n', "starting time: ", str(datetime.now()), '\n'])

daq_error_counts = [0, 0, 0, 0]
febd_error_counts = [0, 0, 0, 0]


read_counts = 0

while True: 
    read_counts += 1
#for read_counts in range(1000):
    if read_counts % (100) == 0:
        print "Read for %d times...\n" % read_counts

    line = []
    for portID in uut.getActivePorts():
        port_counts = uut.getPortCounts(portID)
        if port_counts[2] != daq_error_counts[portID]:
	    line = line + [str(datetime.now()), '\t']
            line = line + ["DAQ port %2d: " % portID, 
			   "TX %8d RX %8d RX error %8d " % port_counts,
			   "Error rate %f %% \n" % (100.0 * port_counts[2] / port_counts [1])]
	    daq_error_counts[portID] = port_counts[2]

    for portID, slaveID in uut.getActiveFEBDs():
        febd_counts = uut.getFEBDCount1(portID,slaveID)
        if febd_counts[2] != febd_error_counts[portID]:
	    line = line + [str(datetime.now()), '\t']
            line = line + ["FEB/D port %2d slave %2d " % (portID, slaveID),
                           "TX %8d RX %8d RX error %8d " % febd_counts,
			   "Error rate %f %% \n" % (100.0 * febd_counts[2] / febd_counts[1])]
	    febd_error_counts[portID] = febd_counts[2]

    if line:
        ofp.writelines(line) 

ofp.close()
