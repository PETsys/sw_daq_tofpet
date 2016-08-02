# -*- coding: utf-8 -*-

## @package atb
# This module defines all the classes, variables and methods to operate the TOFPET ASIC via a UNIX socket  

from math import log, ceil, floor
from bitarray import bitarray
from time import sleep, time
from random import randrange
from sys import stderr
import socket
import struct
from time import time
from posix_ipc import SharedMemory
import mmap
import os
import struct
from subprocess import Popen, PIPE
from sys import maxint, stdout
import tempfile

import DSHM

import tofpet
import sticv3

# WARNING: non final
MAX_DAQ_PORTS = 16
MAX_CHAIN_FEBD = 2
MAX_FEBD_ASIC = 16
MAX_FEBD_HVCHANNEL = 64

###	
## Required for compatibility with configuration files and older code
##
AsicConfig = tofpet.AsicConfig
AsicGlobalConfig = tofpet.AsicGlobalConfig
AsicChannelConfig = tofpet.AsicChannelConfig
intToBin = tofpet.intToBin
###

## A class that instances the classes related to one ASIC configuration and adds additional variables required to configure every ASIC in the system  
class BoardConfig:
        ## Constructor
	def __init__(self):
                self.asicConfigFile = [ "Default Configuration" for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_ASIC) ]
                self.asicBaselineFile = [ "None" for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_ASIC) ]
                self.HVDACParamsFile = "None"
                self.asicConfig = [ None for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_ASIC) ]
		self.hvBias = [ None for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_HVCHANNEL) ]
		self.hvParam = [ (1.0, 0.0) for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_HVCHANNEL)]
		self.channelMap = {}
		self.triggerZones = set()
		self.triggerMinimumToT = 150E-9
		self.triggerCoincidenceWindow = 25E-9
		self.triggerPreWindow = 6.25E-9
		self.triggerPostWindow = 100E-9
		self.triggerSinglesFraction = 0
		return None

        
        ## Writes formatted text file with all parameters and additional information regarding the state of the system  
        # @param prefix Prefix of the file name to be written (it will have the suffix .params)
        def writeParams(self, prefix, comments):
          activeAsicsIDs = [ i for i, ac in enumerate(self.asicConfig) if isinstance(ac, tofpet.AsicConfig) ]
          minAsicID = min(activeAsicsIDs);
    
          defaultAsicConfig = AsicConfig()
          
          global_params= self.asicConfig[minAsicID].globalConfig.getKeys()
          channel_params= self.asicConfig[minAsicID].channelConfig[0].getKeys()
	  
          

          f = open(prefix+'.params', 'w')
	  
	  f.write("--------------------\n")
          f.write("----  COMMENTS  ----\n")
          f.write("--------------------\n\n")
	  f.write(comments)

	  f.write("\n\n")

          f.write("--------------------\n")
          f.write("-- DEFAULT PARAMS --\n")
          f.write("--------------------\n\n")
          f.write("Global{\n")    
 
        
          for i,key in enumerate(global_params):
            for ac in self.asicConfig:
              if not isinstance(ac, tofpet.AsicConfig): continue
              value= ac.globalConfig.getValue( key)
              value_d= defaultAsicConfig.globalConfig.getValue(key)
              if(value_d==value):
                check=True
              else:
                check=False
                break    
            if check:
               f.write('\t"%s" : %d\n' % (key, value))         
   
          f.write("\t}\n")    
          f.write("\n") 
          f.write("Channel{\n")    
    
          for i,key in enumerate(channel_params):
            check=True  
            for ac in self.asicConfig:
              if not isinstance(ac, tofpet.AsicConfig): continue
              if not check:
                break 
              for ch in range(64):
                value= ac.channelConfig[ch].getValue(key)
                value_d= defaultAsicConfig.channelConfig[ch].getValue(key)
                if(value_d==value):
                  check=True 
                else:
                  check=False
                  break    
            if check:
              f.write('\t"%s" : %d\n' % (key, value))
	


          f.write("\t}\n\n")  
          f.write("------------------------\n")
          f.write("-- NON-DEFAULT PARAMS --\n")    
          f.write("------------------------\n")
          ac_ind=0
          for ac in self.asicConfig:
            if not isinstance(ac, tofpet.AsicConfig): continue
        
            f.write("ASIC%d.Global{\n"%ac_ind)  
            for i,key in enumerate(global_params):
              value= ac.globalConfig.getValue(key)
              value_d= defaultAsicConfig.globalConfig.getValue(key)
              if(value_d!=value):
                f.write('\t"%s" : %d\n' % (key, value)) 
            f.write("\t}\n\n")
        
            f.write("ASIC%d.ChAll{\n"%ac_ind)  
            key_list = []
            for i,key in enumerate(channel_params):
              prev_value=ac.channelConfig[minAsicID].getValue(key)
              for ch in range(64):
                value= ac.channelConfig[ch].getValue(key)   
                value_d= defaultAsicConfig.channelConfig[ch].getValue(key)
                if((value_d!=value) and (value==prev_value)):
                  check=True 
                  prev_value=value
                else:
                  check=False
                  if(value_d!=value):
                    key_list.append(key)
                  break    
              if check:
		f.write('\t"%s" : %d\n' % (key, value))
                
            prev_baseline=ac.channelConfig[minAsicID].getBaseline()
            for ch in range(64):
              baseline= ac.channelConfig[ch].getBaseline()  
              if(baseline==prev_baseline):
                check=True 
              else:
                check=False
            if check:
              f.write("\tBASELINE : %d\n" %  baseline)

            f.write("\t}\n\n")
            
            if not check:
              for ch in range(64):
                f.write("ASIC%d.Ch%d{\n"%(ac_ind,ch))
                for key in key_list:
                  value= ac.channelConfig[ch].getValue(key)
                  f.write('\t"%s" : %d\n' % (key, value))
                baseline= ac.channelConfig[ch].getBaseline()
                f.write("\tBASELINE : %d\n"%baseline)
                f.write("\t}\n")
	      f.write("\n")
            ac_ind+=1


          f.write("\n") 
          f.write("--------------------------------------\n")
          f.write("-- CONFIGURATION and BASELINE FILES --\n")
          f.write("--------------------------------------\n\n")
          f.write("HVDAC File: %s\n"%self.HVDACParamsFile)
          asic_id=0
          for filename in self.asicConfigFile:
            f.write("ASIC%d Configuration File: %s\n"%(asic_id,filename)) 
            asic_id+=1
          asic_id=0
          for filename in self.asicBaselineFile:
            f.write("ASIC%d Baseline File: %s\n"%(asic_id,filename))
            asic_id+=1
          f.write("\n")
          f.write("-------------\n")
          f.write("-- HV BIAS --\n")
          f.write("-------------\n\n")

	  for i,entry in enumerate(self.hvBias):
            if entry!= None:
              f.write("%d\t%f"%(i,entry))
	      f.write("\n")
          f.close()



def intToBin(v, n, reverse=False):
	v = int(v)
	if v < 0: 
		v = 0

	if v > 2**n-1:
		v = 2**n-1

	s = bitarray(n, endian="big")
	for i in range(n):
		s[n-i-1] = (v >> i) & 1 != 0
	if reverse:
		s.reverse();
	return s 
	
def binToInt(s, reverse=False):
	if reverse:
		s.reverse()
	r = 0
	n = len(s)
	for i in range(n):
		r += s[i] * 2**(n-i-1)
	return r
	
def grayToBin(g):
	b = bitarray(len(g))
	b[0] = g[0]
	for i in range(1, len(g)):
		b[i] = b[i-1] != g[i]
	return b
	
def grayToInt(v):
	return binToInt(grayToBin(v))
	

## Exception: a command to FEB/D was sent but a reply was not received.
#  Indicates a communication problem.
class CommandErrorTimeout(Exception):
	def __init__(self, portID, slaveID):
		self.addr = portID, slaveID
	def __str__(self):
		return "Time out from FEB/D at port %2d, slave %2d" % self.addr
## Exception: The number of ASIC data links read from FEB/D is invalid.
#  Indicates a communication problem or bad firmware in FEB/D.
class ErrorInvalidLinks(Exception):
	def __init__(self, portID, slaveID, value):
		self.addr = value, portID, slaveID
	def __str__(self):
		return "Invalid NLinks value (%d) from FEB/D at port %2d, slave %2d" % self.addr
## Exception: the ASIC type ID read from FEB/D is invalid.
#  Indicates a communication problem or bad firmware in FEB/D.
class ErrorInvalidAsicType(Exception): 
	def __init__(self, portID, slaveID, value):
		self.addr = portID, slaveID, value
	def __str__(self):
		return "Invalid ASIC type FEB/D at port %2d, slave %2d: %016llx" % self.addr
## Exception: no active FEB/D was found in any port.
#  Indicates that no FEB/D is plugged and powered on or that it's not being able to establish a data link.
class ErrorNoFEB(Exception):
	def __str__(self):
		return "No active FEB/D on any port"

## Exception: testing for ASIC presence in FEB/D has returned a inconsistent result.
#  Indicates that there's a FEB/A board is not properly plugged or a hardware problem.
class ErrorAsicPresenceInconsistent(Exception):
	def __init__(self, portID, slaveID, asicID, s):
		self.__data = (portID, slaveID, asicID, s)
	def __str__(self):
		return "ASIC at port %2d, slave %2d, asic %2d has inconsistent state: %s" % (self.__data)

## Exception: testing for ASIC presence in FEB/D has changed state after system initialization.
#  Indicates that there's a FEB/A board is not properly plugged or a hardware problem.
class ErrorAsicPresenceChanged(Exception):
	def __init__(self, portID, slaveID, asicID):
		self.__data = (portID, slaveID, asicID)
	def __str__(self):
		return "ASIC at port %2d, slave %2d, asic %2d changed state" % (self.__data)
	
class TMP104CommunicationError(Exception):
	def __init__(self, portID, slaveID, din, dout):
		self.__portID = portID
		self.__slaveID = slaveID
		self.__din = din
		self.__dout = dout
	def __str__(self):
		return "TMP104 read error at port %d, slave %d. Chain looks open. (Data: IN = %s, OUT = %s)" % (self.__portID, self.__slaveID, [ hex(x) for x in self.__din ], [ hex(x) for x in self.__dout ])

class ClockNotOK(Exception):
	def __init__(self, portID, slaveID):
		self.__portID = portID
		self.__slaveID = slaveID
	def __str__(self):
		return "Clock not locked at port %d, slave %d" % (self.__portID, self.__slaveID)
	
## A class that contains all methods related to connection, control and data transmission to/from the system via the "daqd" interface	
class ATB:
        ## Constructor, sets the UNIX socket parameters and other data transmission properties
        # @param socketPath Should match the socket name in daqd, which is by default "/tmp/d.sock"
        # @param debug When set to true, enables the printing of debug messages
        # @param F Frequency of the clock, default is 160 MHz
	def __init__(self, socketPath, debug=False, F=160E6):
		self.__socketPath = socketPath
		self.__socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
		self.__socket.connect(self.__socketPath)
		self.__lastSN = randrange(0, 2**15-1)
		self.__pendingReplies = 0
		self.__recvBuffer = bytearray([]);
		self.__debug = debug
		self.__dataFramesIndexes = []
		self.__sync0 = 0
		self.__lastSync = 0
		self.__frameLength = 1024.0 / F
		shmName, s0, p1, s1 = self.__getSharedMemoryInfo()
		self.__dshm = DSHM.SHM(shmName)
		#self.__shmParams = (s0, p1, s1)
		#self.__shm = SharedMemory(shmName)
		#self.__shmmap = mmap.mmap(self.__shm.fd, self.__shm.size)
		#os.close(self.__shm.fd)
		self.config = None
		self.__activePorts = []
		self.__activeFEBDs = []
		self.__activeAsics = [ False for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_ASIC) ]
		self.__asicType = [ None for x in range(MAX_DAQ_PORTS * MAX_CHAIN_FEBD * MAX_FEBD_ASIC) ]
		self.__asicConfigCache = {}		
		self.__initOK = False
		self.__tempChannelMapFile = None
		self.__tempTriggerMapFile = None

		# Gating and coincidence trigger will break idle time calculation
		# If either is enabled, they set this flag to non-zero
		self.__idleTimeBreakage = 0x0
		return None


        ## Starts the data acquisition
	# @param mode If set to 1, only data frames  with at least one event are transmitted. If set to 2, all data frames are transmitted. If set to 0, the data transmission is stopped
	def start(self,):
		nTry = 0
		while True:
			try:
				return self.__start()
			except ErrorAsicPresenceChanged as e:
				nTry = nTry + 1;
				if nTry > 5: raise e
				print "WARNING: ", e, ", retrying..."

	def _daqdMode(self, mode):
		template1 = "@HH"
		template2 = "@H"
		n = struct.calcsize(template1) + struct.calcsize(template2);
		data = struct.pack(template1, 0x01, n) + struct.pack(template2, mode)
		self.__socket.send(data)
		data = self.__socket.recv(2)
		assert len(data) == 2		

	def __start(self):
		# First, generate a "sync" in the FEB/D (or ML605) itself
		for portID, slaveID in self.getActiveFEBDs():
			self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))

		for portID, slaveID in self.getActiveFEBDs(): 
			self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)

		# Start acquisition process
		# Disable DAQ card
		self._daqdMode(0);
		# Enable all FEB/D to receive external sync
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 10, 1)
		# Enable DAQ card (also generates sync to FEB/Ds
		self._daqdMode(2);
		# Inhibit all FEB/Ds from receiving external sync
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 10, 0)
		sleep(0.120)

		# Check the status from all the ASICs
		for portID, slaveID in self.getActiveFEBDs():
			nLocalAsics = len(self.getGlobalAsicIDsForFEBD(portID, slaveID))
			
			deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
			decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

			deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(nLocalAsics) ]
			decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(nLocalAsics) ]

			for i, globalAsicID in enumerate(self.getGlobalAsicIDsForFEBD(portID, slaveID)):
				asicType = self.__asicType[globalAsicID]
				if asicType == 0x00020003: 
					# Do not check presense for STiCv3
					continue

				duplet = (deserializerStatus[i], decoderStatus[i])
				if duplet == (True, True):
					localOK = True
				elif duplet == (False, False):
					localOK = False
				else:
					localOK = None
				globalOK = self.__activeAsics[globalAsicID]
				if localOK != globalOK:
					raise ErrorAsicPresenceChanged(portID, slaveID, i)

		return None
       
        ## Stops the data acquisition, the same as calling start(mode=0)
	def stop(self):
		self._daqdMode(0)
		return None

        ## Returns the size of the allocated memory block 
	def __getSharedMemorySize(self):
		return self.__dshm.getSizeInBytes()
		#return self.__shm.size

        ## Returns the name of the shared memory block
	def __getSharedMemoryName(self):
		name, s0, p1, s1 =  self.__getSharedMemoryInfo()
		return name

        ## Returns array info 
	def __getSharedMemoryInfo(self):
		template = "@HH"
		n = struct.calcsize(template)
		data = struct.pack(template, 0x02, n)
		self.__socket.send(data);

		template = "@HQQQ"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		length, s0, p1, s1 = struct.unpack(template, data)
		name = self.__socket.recv(length - n);
		return (name, s0, p1, s1)

	## Returns an array with the active ports (PAB only has port 0)
	def getActivePorts(self):
		if self.__activePorts == []:
			self.__activePorts = self.__getActivePorts()
		return self.__activePorts

	def __getActivePorts(self):
		template = "@HH"
		n = struct.calcsize(template)
		data = struct.pack(template, 0x06, n)
		self.__socket.send(data);

		template = "@HQ"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		length, mask = struct.unpack(template, data)
		reply = [ n for n in range(12*16) if (mask & (1<<n)) != 0 ]
		return reply

	## Returns an array of (portID, slaveID) for the active FEB/Ds (PAB) 
	def getActiveFEBDs(self):
		if self.__activeFEBDs == []:
			self.__activeFEBDs = self.__getActiveFEBDs()
		return self.__activeFEBDs

	def __getActiveFEBDs(self):
		r = []
		for portID in self.getActivePorts():
			r.append((portID, 0))
			slaveID = 0
			while self.readFEBDConfig(portID, slaveID, 0, 12) != 0x0:
				slaveID += 1
				r.append((portID, slaveID))
			
		return r

	def __getPossibleFEBDs(self):
		return [ (portID, slaveID) for portID in range(MAX_DAQ_PORTS) for slaveID in range(MAX_CHAIN_FEBD) ]

	## Returns an array with the IDs of the active ASICS
	def getActiveAsics(self):
		return [ i for i, active in enumerate(self.__activeAsics) if active ]

	def getActiveTOFPETAsics(self):
		return [ i for i, active in enumerate(self.__activeAsics) if active and self.__asicType[i] == 0x00010001 ]

	## Returns a 3 element tupple with the number of transmitted, received, and error packets for a given port 
	# @param port The port for which to get the desired output 
	def getPortCounts(self, port):
		template = "@HHH"
		n = struct.calcsize(template)
		data = struct.pack(template, 0x07, n, port)
		self.__socket.send(data);

		template = "@HQQQ"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		length, tx, rx, rxBad = struct.unpack(template, data)		
		tx = binToInt(grayToBin(intToBin(tx, 48)))
		rx = binToInt(grayToBin(intToBin(rx, 48)))
		rxBad = binToInt(grayToBin(intToBin(rxBad, 48)))
		return (tx, rx, rxBad)

	## Returns a 3 element tupple with the number of transmitted, received, and error packets for a given FEB/D
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	def getFEBDCount1(self, portID, slaveID):
		mtx = self.readFEBDConfig(portID, slaveID, 1, 0)
		mrx = self.readFEBDConfig(portID, slaveID, 1, 1)
		mrxBad = self.readFEBDConfig(portID, slaveID, 1, 2)

		slaveOn = self.readFEBDConfig(portID, slaveID, 0, 12) != 0x0

		stx = self.readFEBDConfig(portID, slaveID, 1, 3)
		srx = self.readFEBDConfig(portID, slaveID, 1, 4)
		srxBad = self.readFEBDConfig(portID, slaveID, 1, 5)
		
		mtx = binToInt(grayToBin(intToBin(mtx, 48)))
		mrx = binToInt(grayToBin(intToBin(mrx, 48)))
		mrxBad = binToInt(grayToBin(intToBin(mrxBad, 48)))
		stx = binToInt(grayToBin(intToBin(stx, 48)))
		srx = binToInt(grayToBin(intToBin(srx, 48)))
		srxBad = binToInt(grayToBin(intToBin(srxBad, 48)))
		return (mtx, mrx, mrxBad, slaveOn, stx, srx, srxBad)

	## Gets the current write and read pointer
	def __getDataFrameWriteReadPointer(self):
		template = "@HH"
		n = struct.calcsize(template)
		data = struct.pack(template, 0x03, n);
		self.__socket.send(data)

		template = "@HII"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		n, wrPointer, rdPointer = struct.unpack(template, data)

		return wrPointer, rdPointer

	def __setDataFrameReadPointer(self, rdPointer):
		template1 = "@HHI"
		n = struct.calcsize(template1) 
		data = struct.pack(template1, 0x04, n, rdPointer);
		self.__socket.send(data)

		template = "@I"
		n = struct.calcsize(template)
		data2 = self.__socket.recv(n);
		r2, = struct.unpack(template, data2)
		assert r2 == rdPointer

		return None
		
        ## Returns a data frame read form the shared memory block
	def getDataFrame(self, nonEmpty=False):
		timeout = 0.5
		t0 = time()
		r = None
		while (r == None) and ((time() - t0) < timeout):
			wrPointer, rdPointer = self.__getDataFrameWriteReadPointer()
#			print "WR/RD pointers = %08x %08x" % (wrPointer, rdPointer)
			bs = self.__dshm.getSizeInFrames()
			while (wrPointer != rdPointer) and (r == None):
				index = rdPointer % bs
				rdPointer = (rdPointer + 1) % (2 * bs)

				nEvents = self.__dshm.getNEvents(index)
				if nEvents == 0 and nonEmpty == True:
					continue;

		
				frameID = self.__dshm.getFrameID(index)
				frameLost = self.__dshm.getFrameLost(index)

				tofpetEvents = [ i for i in range(nEvents) if self.__dshm.getEventType(index, i) == 0 ]
				events = []
				for i in tofpetEvents:
					events.append((	self.__dshm.getAsicID(index, i), \
							self.__dshm.getChannelID(index, i), \
							self.__dshm.getTACID(index, i), \
							self.__dshm.getTCoarse(index, i), \
							self.__dshm.getECoarse(index, i), \
							self.__dshm.getTFine(index, i), \
							self.__dshm.getEFine(index, i), \
							self.__dshm.getChannelIdleTime(index, i), \
							self.__dshm.getTACIdleTime(index, i), \
						))

				r = { "id" : frameID, "lost" : frameLost, "events" : events }

#			print "Setting rdPointer to %08x\n" %rdPointer
			self.__setDataFrameReadPointer(rdPointer)

		return r

	def printDataFrame(self, nonEmpty=False):
		timeout = 0.5
		t0 = time()
		r = False
		while (r == False) and ((time() - t0) < timeout):
			wrPointer, rdPointer = self.__getDataFrameWriteReadPointer()
#			print "WR/RD pointers = %08x %08x" % (wrPointer, rdPointer)
			bs = self.__dshm.getSizeInFrames()
			while (wrPointer != rdPointer) and (r == False):
				index = rdPointer % bs
				rdPointer = (rdPointer + 1) % (2 * bs)

				nEvents = self.__dshm.getNEvents(index)
				if nEvents == 0 and nonEmpty == True:
					continue;

				r = True
				frameID = self.__dshm.getFrameID(index)
				frameSize = self.__dshm.getFrameSize(index)
				print "DATA FRAME FOUND"
				print "ID = %12d SIZE = %4d words" % (frameID, frameSize)
				print "BEGIN CONTENT"
				for n in range(frameSize):
					print "WORD %4d %016x" % (n, self.__dshm.getFrameWord(index, n))
				print "END CONTENT"

			self.__setDataFrameReadPointer(rdPointer)

		return r
	
	## Sends a command to the FPGA
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
        # @param commandType Information for the FPGA firmware regarding the type of command being transmitted
	# @param payload The actual command to be transmitted
        # @param maxTries The maximum number of attempts to send the command without obtaining a valid reply   	
	def sendCommand(self, portID, slaveID, commandType, payload, maxTries=10):
		nTries = 0;
		reply = None
		doOnce = True
		while doOnce or (reply == None and nTries < maxTries):
			doOnce = False

			nTries = nTries + 1
			if nTries > 5: print "Timeout sending command. Retry %d of %d" % (nTries, maxTries)

			sn = self.__lastSN
			self.__lastSN = (sn + 1) & 0x7FFF

			rawFrame = bytearray([ portID & 0xFF, slaveID & 0xFF, (sn >> 8) & 0xFF, (sn >> 0) & 0xFF, commandType]) + payload
			rawFrame = str(rawFrame)

			#print [ hex(ord(x)) for x in rawFrame ]

			template1 = "@HH"
			n = struct.calcsize(template1) + len(rawFrame)
			data = struct.pack(template1, 0x05, n)
			self.__socket.send(data)
                        
			self.__socket.send(rawFrame);

			template2 = "@H"
			n = struct.calcsize(template2)
			data = self.__socket.recv(n)
			nn, = struct.unpack(template2, data)

			if nn < 4:
				continue

                       # print "Trying to read %d bytes" % nn
			data = self.__socket.recv(nn)
			#print [ "%02X" % x for x in bytearray(data) ]

			data = data[3:]
			reply = bytearray(data)
			

		if reply == None:
			print reply
			raise CommandErrorTimeout(portID, slaveID)

		return reply
	
		
		
        ## Writes in the FPGA register (Clock frequency, etc...)
        # @param regNum Identification of the register to be written
        # @param regValue The value to be written
	def setSI53xxRegister(self, regNum, regValue):
		reply = self.sendCommand(0, 0, 0x02, bytearray([0b00000000, regNum]))	
		reply = self.sendCommand(0, 0, 0x02, bytearray([0b01000000, regValue]))
		reply = self.sendCommand(0, 0, 0x02, bytearray([0b10000000]))
		return None
	
	
	## Defines all possible commands structure that can be sent to the ASIC and calls for sendCommand to actually transmit the command
        # @param asicID Identification of the ASIC that will receive the command
	# @param command Command type to be sent. The list of possible keys for this parameter is hardcoded in this function
        # @param value The actual value to be transmitted to the ASIC if it applies to the command type   
        # @param channel If the command is destined to a specific channel, this parameter sets its ID. 	  
	def doTOFPETAsicCommand(self, asicID, command, value=None, channel=None):
		nTry = 0
		while True:
			try:
				return self.__doTOFPETAsicCommand(asicID, command, value=value, channel=channel)
			except tofpet.ConfigurationError as e:
				nTry = nTry + 1
				if nTry >= 5:
					raise e


	## Defines all possible commands structure that can be sent to the ASIC and calls for sendCommand to actually transmit the command
        # @param asicID Identification of the ASIC that will receive the command
	# @param command Command type to be sent. The list of possible keys for this parameter is hardcoded in this function
        # @param value The actual value to be transmitted to the ASIC if it applies to the command type   
        # @param If the command is destined to a specific channel, this parameter sets its ID. 
	def __doTOFPETAsicCommand(self, asicID, command, value=None, channel=None):
		commandInfo = {
		#	commandID 		: (code,   ch,   read, data length)
			"wrChCfg"		: (0b0000, True, False, 53),
			"rdChCfg"		: (0b0001, True, True, 53),
			"wrChTCfg"		: (0b0010, True, False, 1),
			"rdChTCfg"		: (0b0011, True, True, 1),
			"rdChDark"		: (0b0100, True, True, 10),
			
			"wrGlobalCfg" 	: (0b1000, False, False, 26+14*6),
			"rdGlobalCfg" 	: (0b1001, False, True, 26+14*6),
			"wrGlobalTCfg"	: (0b1100, False, False, 7),
			"rdGlobalTCfg"	: (0b1101, False, True, 7),
			"wrTestPulse"	: (0b1010, False, False, 10+8+8)
		}
	
		commandCode, isChannel, isRead, dataLength = commandInfo[command]

		# Avoid re-uploading this configuration if it's already in the chip
		cacheKey = (command, asicID, channel)
		if not isRead:
			try:
				lastValue = self.__asicConfigCache[cacheKey]
				if value == lastValue:
					return (0, None)
			except KeyError:
				pass

		byte0 = [ (commandCode << 4) + (asicID & 0x0F) ]
		if isChannel:
				
			byte1 = [ (channel) & 0xFF ]
			
		else:
			byte1 = []
			
		
		if not isRead:
			assert len(value) == dataLength
			nBytes = int(ceil(dataLength / 8.0))
			paddedValue = value + bitarray([ False for x in range((nBytes * 8) - dataLength) ])
			byteX = [ ord(x) for x in paddedValue.tobytes() ]
		else:
			byteX = []
		
		cmd = bytearray(byte0 + byte1 + byteX)

		portID, slaveID, lAsicID  = self.asicIDGlobalToTuple(asicID)


		reply = self.sendCommand(portID, slaveID, 0x00, cmd)
		if len(reply) < 2: raise tofpet.ConfigurationErrorBadReply(2, len(reply))
		status = reply[1]
			

		if status == 0xE3:
			raise tofpet.ConfigurationErrorBadAck(portID, slaveID, lAsicID, 0)
		elif status == 0xE4:
			raise tofpet.ConfigurationErrorBadCRC(portID, slaveID, lAsicID )
		elif status == 0xE5:
			raise tofpet.ConfigurationErrorBadAck(portID, slaveID, lAsicID, 1)
		elif status != 0x00:
			raise tofpet.ConfigurationErrorGeneric(portID, slaveID, lAsicID , status)

		if isRead:
			#print [ "%02X" % x for x in reply ]
			expectedBytes = ceil(dataLength/8)
			if len(reply) < (2+expectedBytes): 
				print len(reply), (2+expectedBytes)
				raise tofpet.ConfigurationErrorBadReply(2+expectedBytes, len(reply))
			reply = str(reply[2:])
			data = bitarray()
			data.frombytes(reply)
			#print data
			return (status, data[0:dataLength])
		else:
			# Check what we wrote
			readCommand = 'rd' + command[2:]
			readStatus, readValue = self.__doTOFPETAsicCommand(asicID, readCommand, channel=channel)
			if readValue != value:
				raise tofpet.ConfigurationErrorBadRead(portID, slaveID, asicID % 16, value, readValue)

			self.__asicConfigCache[cacheKey] = bitarray(value)
			return (status, None)


	## Turns on LDO for STICv3 FEB/A boards
	# @param on Turn ON (True) or OFF (False). Turning ON is deprecated
	def febAOnOff(self, on = False):
		if on == True:
			print "WARNING: Ignoring request to turn ON LDO for FEB/A boards. They will be turned on during initialize()"
		else:
			print "INFO: Turning OFF LDO for FEB/A boards"
			for portID, slaveID in self.getActiveFEBDs():
				self.writeFEBDConfig(portID, slaveID, 0, 5, 0x00);
	

	## Returns list globalAsicIDs belonging to this FEB/D
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	def getGlobalAsicIDsForFEBD(self, portID, slaveID):
		n = slaveID * MAX_DAQ_PORTS + portID
		return [x for x in range(n * MAX_FEBD_ASIC, (n+1) * MAX_FEBD_ASIC)]
	
	## Returns a tuple with the (portID, slaveID, localAsicID) for which an globalAsicID belongs
	# @param asicID Global ASIC ID
	def asicIDGlobalToTuple(self, asicID):
		slaveID = asicID / (MAX_DAQ_PORTS * MAX_FEBD_ASIC)
		r = asicID % (MAX_DAQ_PORTS * MAX_FEBD_ASIC)
		portID = r / MAX_FEBD_ASIC
		asicID = r % MAX_FEBD_ASIC
		return (portID, slaveID, asicID)

	
	def __initializeFEBD_TOFPET(self, portID, slaveID):
		print "INFO: FEB/D at port %d, slave %d is of type TOFPET" % (portID, slaveID)
		# Disable data reception logic
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0x0)
		# Disable test pulse
		self.setTestPulseNone()
		# Send a 64K clock long reset
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0xFF, 0xFF]))
		sleep(0.120 + 64*self.__frameLength)	# Need to wait for at least 100 ms, plus the reset time
		# Reset the ASIC config cache, as we've just reset the ASICs too
		self.__asicConfigCache = {}
		# Read the number of ASIC data links expected by this FEB/D
		# and generate a suitable default global configuration
		nLinks = self.readFEBDConfig(portID, slaveID, 0, 1)
		defaultGlobalConfig = tofpet.AsicGlobalConfig()
		defaultGlobalConfig.setValue("ddr_mode", 1)
		if nLinks == 1:
			defaultGlobalConfig.setValue("tx_mode", 0)
		elif nLinks == 2:
			defaultGlobalConfig.setValue("tx_mode", 1)
		else:
			raise ErrorInvalidLinks(portID, slaveID, nLinks)
		
		# Try to configure each of the possible ASICs
		localAsicIDList = self.getGlobalAsicIDsForFEBD(portID, slaveID)
		localAsicConfigOK = [ False for x in localAsicIDList ]
		for i, asicID in enumerate(localAsicIDList):
			try:
				self.doTOFPETAsicCommand(asicID, "wrGlobalCfg", value=defaultGlobalConfig)
				localAsicConfigOK[i] = True
			# Either of these two errors will occur when an ASIC is not present
			# So we use them as indicationt the ASIC is not there
			except tofpet.ConfigurationErrorBadAck as e:
				pass
			except tofpet.ConfigurationErrorStuckHigh as e:
				pass

		# Enable the reception logic	
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)
		#self.sendCommand(portID, slaveID, 0x03, bytearray([0x04, 0x00, 0x00]))
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))
		sleep(0.120)	# Need to wait for at least 100 ms, plus the reset time

		for i, asicID in enumerate(localAsicIDList):
			if not localAsicConfigOK[i]: continue

			status, readback = self.doTOFPETAsicCommand(asicID, "rdGlobalCfg")
			if readback != defaultGlobalConfig:
				raise tofpet.ConfigurationErrorBadRead(portID, slaveID, i, defaultGlobalConfig, readback)


		deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
		decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

		deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		
		for i, asicID in enumerate(localAsicIDList):
			triplet = (localAsicConfigOK[i], deserializerStatus[i], decoderStatus[i])
			self.__asicType[asicID] = 0x00010001
			if triplet == (True, True, True):
				# All OK, ASIC is present and OK
				self.__activeAsics[asicID] = True
			elif triplet == (False, False, False):
				# All failed, ASIC is not present
				self.__activeAsics[asicID] = False
			else:
				# Something failed!!
				raise ErrorAsicPresenceInconsistent(portID, slaveID, i, str(triplet))

		return None

	def __initializeFEBD_STICv3(self, portID, slaveID):
		print "INFO: FEB/D at port %d, slave %d is of type STICv3" % (portID, slaveID)
		# Disable data reception logic
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0x0)
		# Disable test pulse
		self.setTestPulseNone()
		# Send a 64K clock long reset
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0xFF, 0xFF]))
		sleep(0.120 + 64* self.__frameLength)	# Need to wait for at least 100 ms, plus the reset time

		# Load a minimum power configuration into the FEB/D
		allOff = sticv3.AsicConfig('stic_configurations/ALL_OFF.txt')
		writeData = allOff.data + "\x00\x00\x00"
		memAddr = 1
		while len(writeData) > 3:
			d = writeData[0:4]
			writeData = writeData[4:]
			msb = (memAddr >> 8) & 0xFF
			lsb = memAddr & 0xFF
			#print "RAM ADDR %4d, nBytes Written = %d, nBytes Left = %d" % (memAddr, len(d), len(data))
			#print [hex(ord(x)) for x in d]
			self.sendCommand(portID, slaveID, 0x00, bytearray([0x00, msb, lsb] + [ ord(x) for x in d ]))
			memAddr = memAddr + 1	

		for n in range(8):
			ldoVector = self.readFEBDConfig(portID, slaveID, 0, 5)
			if ldoVector & (1<<n) == 0: # This LDO is OFF, let's turn it on	
				print "INFO: LDO %d was OFF, turning ON" % n
				ldoVector = ldoVector | (1<<n)
				self.writeFEBDConfig(portID, slaveID, 0, 5, ldoVector)
				for i in range(16): # Apply the ALL_OFF configuration to all chips in FEB/D
					sleep(0.010)
					self.sendCommand(portID, slaveID, 0x00, bytearray([0x01, i]))

		for i in range(16): # Apply the ALL_OFF configuration to all chips in FEB/D
			self.sendCommand(portID, slaveID, 0x00, bytearray([0x01, i]))
			self.sendCommand(portID, slaveID, 0x00, bytearray([0x01, i]))
			readBackData = self.readConfigSTICv3(portID, slaveID)


			nBytesToCompare = min(len(allOff.data), len(readBackData))
			readBackData[nBytesToCompare-1]=readBackData[nBytesToCompare-1] & 0x01

			if readBackData[0:nBytesToCompare] != allOff.data[0:nBytesToCompare]:
				print "Config mismatch for P%02d S%02d A%02d" % (portID, slaveID, i)
				print "Wrote ", [ "%02x" % ord(x) for x in allOff.data[0:nBytesToCompare]]
				print "Read  ", [ "%02x" % ord(x) for x in str(readBackData[0:nBytesToCompare])]

			sleep(0.010)


		# Enable the reception logic	
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)
		#self.sendCommand(portID, slaveID, 0x03, bytearray([0x04, 0x00, 0x00]))
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0xFF]))
		sleep(0.120)	# Need to wait for at least 100 ms, plus the reset time

		localAsicIDList = self.getGlobalAsicIDsForFEBD(portID, slaveID)

		deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
		decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

		deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(len(localAsicIDList)) ]
		decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(len(localAsicIDList)) ]

		inconsistentAsicException = None
		for i, asicID in enumerate(localAsicIDList):
			self.__asicType[asicID] = 0x00020003

			# All STiC ASICs are assumed to be true
			self.__activeAsics[asicID] = True
			continue

			duplet = (deserializerStatus[i], decoderStatus[i])
			if duplet == (True, True):
				# All OK, ASIC is present and OK
				self.__activeAsics[asicID] = True
			elif duplet == (False, False):
				# All failed, ASIC is not present
				self.__activeAsics[asicID] = False
			else:
				# Something failed!!
				self.__activeAsics[asicID] = None
				inconsistentAsicException = ErrorAsicPresenceInconsistent(portID, slaveID, i, str(duplet))
				print inconsistentAsicException

		if inconsistentAsicException is not None:
			raise inconsistentAsicException

		return None

	## Sends the entire configuration (needs to be assigned to the abstract ATB.config data structure) to the ASIC and starts to write data to the shared memory block
        # @param maxTries The maximum number of attempts to read a valid dataframe after uploading configuration 
	def initialize(self, maxTries = 1):
		self.__activePorts = self.__getActivePorts()
		self.__activeFEBDs = self.__getActiveFEBDs()
		assert self.config is not None
		activePorts = self.getActivePorts()
		print "INFO: active FEB/D on ports: ", (", ").join([str(x) for x in activePorts])
		# Stop acquisition (makes it easier to send commands)
		self.stop()		
		sleep(0.5)

		self.__setIdleTimeComputation(True)
		self.__setSortingMode(True)
		self.disableTriggerGating()
		self.setTestPulseNone()
		self.__disableCoincidenceTrigger()
		for portID, slaveID in self.getActiveFEBDs():
			coreClockNotOK = self.readFEBDConfig(portID, slaveID, 0, 11)
			if coreClockNotOK != 0x0:
				raise ClockNotOK(portID, slaveID)

			asicType = self.readFEBDConfig(portID, slaveID, 0, 0)
			nTry = 0
			while True:
				try:
					if asicType == 0x00010001:
						self.__initializeFEBD_TOFPET(portID, slaveID)
					elif asicType == 0x00020003:
						self.__initializeFEBD_STICv3(portID, slaveID)
					else:
						raise ErrorInvalidAsicType(portID, slaveID, asicType)
					break
				except ErrorAsicPresenceInconsistent as e:
					nTry = nTry + 1
					if nTry > 5: raise e
					print "WARNING ", e, ", retrying..."


		self.uploadConfig()
		self.start()
		sleep(0.120)
		self.doSync()
		self.__initOK = True

	def __getCurrentFrameID(self):
		activePorts = self.getActivePorts()
		if activePorts == []:
			raise ErrorNoFEB()

		portID = min(activePorts)
		gray = self.readFEBDConfig(portID, 0, 0, 7)
		timeTag = binToInt(grayToBin(intToBin(gray, 46)))
		frameID = timeTag / 1024
		return frameID

        ## Discards all data frames which may have been generated before the function is called. Used to synchronize data reading with the effect of previous configuration commands.
	def doSync(self, clearFrames=True):
		while True:	
			targetFrameID = self.__getCurrentFrameID()
			#print "Waiting for frame %1d" % targetFrameID
			while True:
				df = self.getDataFrame()
				assert df != None
				if df == None:
					continue;

				if  df['id'] > targetFrameID:
					#print "Found frame %d (%f)" % (df['id'], df['id'] * self.__frameLength)
					break

				# Set the read pointer to write pointer, in order to consume all available frames in buffer
				wrPointer, rdPointer = self.__getDataFrameWriteReadPointer();
				self.__setDataFrameReadPointer(wrPointer);

			# Do this until we took less than 100 ms to sync
			currentFrameID = self.__getCurrentFrameID()
			if (currentFrameID - targetFrameID) * self.__frameLength < 0.100:
				break
		

		return
	  

	## Returns a (portID, slaveID, localID) tuple for a globalHVChannel
	# @param globalHVChannelID Global HV channel ID
	def hvChannelGlobalToTulple(self, globalHVChannelID):
		slaveID = globalHVChannelID / ( MAX_DAQ_PORTS * MAX_FEBD_HVCHANNEL) 
		r = globalHVChannelID % ( MAX_DAQ_PORTS * MAX_FEBD_HVCHANNEL) 
		portID = r / MAX_FEBD_HVCHANNEL
		hvChannelID = r % MAX_FEBD_HVCHANNEL
		return (portID, slaveID, hvChannelID)

	## Return a list of global HV channel IDs for a FEB/D
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	def getGlobalHVChannelIDForFEBD(self, portID, slaveID):
		n = slaveID * MAX_DAQ_PORTS + portID		
		return [ x for x in range(n * MAX_FEBD_HVCHANNEL, (n+1) * MAX_FEBD_HVCHANNEL) ]

	## Sets all HV channels
	# @param voltageRequested Voltage to be set
	def setAllHVDAC(self, voltageRequested):
		for portID, slaveID in self.getActiveFEBDs():
			for globalHVChannelID in self.getGlobalHVChannelIDForFEBD(portID, slaveID):
				if self.config != None:
					self.config.hvBias[globalHVChannelID] = voltageRequested
				self.setHVDAC(globalHVChannelID, voltageRequested)
				
	

        ## Sets the HVDAC voltage using calibration data 
        # @param channel The HVDAC channel to be set
        # @param voltageRequested The voltage to be set in units of Volts, using the calibration
	def setHVDAC(self, channel, voltageRequested):
		m, b = self.config.hvParam[channel]
		voltage = voltageRequested* m + b
		#print "%4d %f => %f, %f => %f" % (channel, voltageRequested, m, b, voltage)
		self.setHVDAC_(channel, voltage)
        
        ## Sets the HVDAC voltage to desired uncalibrated value 
        # @param channel The HVDAC channel to be set
        # @param voltage The voltage to be set in units of Volts
	def setHVDAC_(self, channel, voltage):
		portID, slaveID, localChannel = self.hvChannelGlobalToTulple(channel)
		if (portID, slaveID) not in self.getActiveFEBDs():			
			print "WARNING: Configuration specified for HV channel %d but FEB/D (P%02d S%02d) is not active. Skipping" % (channel, portID, slaveID)
			return

		voltage = int(voltage * 2**14 / (50 * 2.048))
		if voltage > 2**14-1:
			voltage = 2**14-1

		if voltage < 0:
			voltage = 0


		whichDAC = localChannel / 32
		channel = localChannel % 32

		whichDAC = 1 - whichDAC # Wrong decoding in ad5535.vhd

		dacBits = intToBin(whichDAC, 1) + intToBin(channel, 5) + intToBin(voltage, 14) + bitarray('0000')
		dacBytes = bytearray(dacBits.tobytes())
		return self.sendCommand(portID, slaveID, 0x01, dacBytes)

	## Disables external gate function
	def disableTriggerGating(self):
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 13, 0x00);
		self.__idleTimeBreakage &= ~0b1
		self.__setIdleTimeComputation(self.__idleTimeBreakage == 0)

	## Enabled external gate function
	# @param delay Delay of the external gate signal, in clock periods
	def enableTriggerGating(self, delay):
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 13, 1024 + delay);
		self.__idleTimeBreakage |= 0b1
		self.__setIdleTimeComputation(self.__idleTimeBreakage == 0)

	def __setCoincidenceTrigger(self, enable, triggerMinimumToT,
					triggerCoincidenceWindow, triggerPreWindow,
					triggerPostWindow, triggerSinglesFraction,
					triggerZones):

		T_seconds = self.__frameLength / 1024
		T_nanoseconds = T_seconds * 1E9

		# Encode parameters
		template1 = "@IIIIII"
		n1 = struct.calcsize(template1)
		data1 = struct.pack(template1,
			enable,
			int(floor(triggerMinimumToT / T_seconds)),
			int(ceil(triggerCoincidenceWindow / T_seconds)),
			int(ceil(triggerPreWindow / T_seconds)),
			int(ceil(triggerPostWindow / T_seconds)),
			int(triggerSinglesFraction)
		)

		# Encode masks
		data2 = ""
		for n in range(32):
			word = 0x00000000
			for a, b in [ t for t in triggerZones if n in t ]:
				# Set the bit for two port ID
				# One of them is the self port, which would enable self triggering..
				word |= 1 << a
				word |= 1 << b
			# Append this port's configuration word
			data2 += struct.pack("@I", word)

		# Encode command header
		template0 = "@HH"
		n0 = struct.calcsize(template0)
		data0 = struct.pack(template0, 0x10, n0 + len(data1) + len(data2))

		# Send everything
		self.__socket.send(data0 + data1 + data2)

		# Receive the reply, a 32 bit 0x00000000
		template0 = "@i"
		n0 = struct.calcsize(template0)
		data0 = self.__socket.recv(n0);
		reply, = struct.unpack(template0, data0)

		if enable and reply != 0:
			# We have successfully enabled a hardware coincidence trigger
			# Thus, we need to disable the idle time calculation
			self.__idleTimeBreakage |= 0b10
		else:
			self.__idleTimeBreakage &= ~0b10
		self.__setIdleTimeComputation(self.__idleTimeBreakage == 0)

		return reply

	## Enables the DAQ system coincidence trigger, based on the settings defined in config
	def __enableCoincidenceTrigger(self):
		status = self.__disableCoincidenceTrigger()
		if status != 0:
			# This system doesn't have a hardware trigger
			# Let's just move on
			return status
	
		# Hardcoded map, which is sufficient for current HW triggers
		hwRegionMap = {}
		for slave in range(2):
			for port in range(12):
				for channel in range(1024):
					gid = (slave * 16 + port) * 1024 + channel
					hwRegionMap[gid] = port
		
		hwTriggerZones = set()
		channelMapItems = self.config.channelMap.items()
		for i, (swChannel1, (swRegion1, xi, yi, x, y, z)) in enumerate(channelMapItems):
			hwRegion1 = hwRegionMap[swChannel1]
			for swChannel2, (swRegion2, xi, yi, x, y, z) in channelMapItems[i:]:
				hwRegion2 = hwRegionMap[swChannel2]
				if (swRegion1, swRegion2) not in self.config.triggerZones:
					continue

				if hwRegion1 == hwRegion2:
					print "INFO: Hardware coincidence trigger available but will not be enabled."
					print "INFO: Reason: coincidences required between region %d and %d but both map to hardware trigger region %d" % (swRegion1, swRegion2, hwRegion1)
					return -1

				hwTriggerZones.add((hwRegion1, hwRegion2))

		status = self.__setCoincidenceTrigger(
			1,
			self.config.triggerMinimumToT,
			self.config.triggerCoincidenceWindow,
			self.config.triggerPreWindow,
			self.config.triggerPostWindow,
			self.config.triggerSinglesFraction,
			hwTriggerZones
		)
		if status == 0:
			print "INFO: Enabled hardware coincidence filter for the following ports: ", list(hwTriggerZones)

		return status
		

	## Disables the DAQ system coincidence trigger (default)
	def __disableCoincidenceTrigger(self):
		return self.__setCoincidenceTrigger(
			0,
			0,
			0,
			0,
			0,
			0,
			set()
		)

        ## Disables test pulse 
	def setTestPulseNone(self):
		cmd =  bytearray([0x01] + [0x00 for x in range(8)])
		for portID, slaveID in self.getActiveFEBDs():
			self.sendCommand(portID, slaveID, 0x03,cmd)

		self.__setSortingMode(True)
		return None

	def __setSortingMode(self, enable):
		if enable: 
			word = 0x1
		else: 
			word = 0x0

		template1 = "@HHI"
		n = struct.calcsize(template1)
		data = struct.pack(template1, 0x09, n, word);
		self.__socket.send(data)

		template = "@I"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		return None

	def __setIdleTimeComputation(self, enable):
		if enable:
			word = 0x1
		else: 
			word = 0x0

		template1 = "@HHI"
		n = struct.calcsize(template1)
		data = struct.pack(template1, 0x11, n, word);
		self.__socket.send(data)

		template = "@I"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		return None

        #def setTestPulseArb(self, invert):
		#if not invert:
			#tpMode = 0b11000000
		#else:
			#tpMode = 0b11100000

		#cmd =  bytearray([0x01] + [tpMode] + [0x00 for x in range(7)])
		#return self.sendCommand(0, 0, 0x03,cmd)
	
        ## Sets the properties of the internal FPGA pulse generator
        # @param length Sets the length of the test pulse, from 1 to 1023 clock periods. 0 disables the test pulse.
        # @param interval Sets the interval between test pulses. The actual interval will be (interval+1)*1024 clock cycles.
        # @param finePhase Defines the delay of the test pulse regarding the start of the frame, in units of 1/392 of the clock.
        # @param invert Sets the polarity of the test pulse: active low when ``True'' and active high when ``False''
	def setTestPulsePLL(self, length, interval, finePhase, invert):
		if not invert:
			tpMode = 0b10000000
		else:
			tpMode = 0b10100000

		finePhase0 = finePhase & 255
		finePhase1 = (finePhase >> 8) & 255
		finePhase2 = (finePhase >> 16) & 255
		interval0 = interval & 255
		interval1 = (interval >> 8) & 255
		length0 = length & 255
		length1 = (length >> 8) & 255
		
		cmd =  bytearray([0x01, tpMode, finePhase2, finePhase1, finePhase0, interval1, interval0, length1, length0])
		for portID, slaveID in self.getActiveFEBDs():
			self.sendCommand(portID, slaveID, 0x03,cmd)

		# DAQ sorter should be disabled when using test pulse triggers
		# Otherwise, we may risk overflowing the sorter's max hits per time slot
		self.__setSortingMode(False)
		return None


	#def loadArbTestPulse(self, address, isPulse, delay, length):
		#if isPulse:
			#isPulseBit = bitarray('1')
		#else:
			#isPulseBit = bitarray('0')

		#delayBits = intToBin(delay, 21)
		#lengthBits = intToBin(length, 10)

		#addressBits = intToBin(address,32)
		
		#bits =  isPulseBit + delayBits + lengthBits + addressBits
		#cmd = bytearray([0x03]) + bytearray(bits.tobytes())
		##print [ hex(x) for x in cmd ]
		#return self.sendCommand(0, 0, 0x03,cmd)

	## Reads a configuration register from a FEB/D
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	# @param addr1 Register block (0..127)
	# @param addr2 Register address (0..255)
	def readFEBDConfig(self, portID, slaveID, addr1, addr2):
		header = [ addr1 & 0x7F, addr2 & 0xFF ]
		data = [ 0x00 for n in range(8)]
		command = bytearray(header + data)

		reply = self.sendCommand(portID, slaveID, 0x05, command);
		
		d = reply[2:]
		value = 0
		for n in range(8):#####  it was 8		
			value = value + (d[n] << (8*n))
		return value

	## Writes a FEB/D configuration register
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	# @param addr1 Register block (0..127)
	# @param addr2 Register address (0..255)
	# @param value The value to written
	def writeFEBDConfig(self, portID, slaveID, addr1, addr2, value):
		header = [ 0x80 | (addr1 & 0x7F), addr2 & 0xFF ]
		data = [ value >> (8*n) & 0xFF for n in range(8) ]
		command = bytearray(header + data)
			
		reply = self.sendCommand(portID, slaveID, 0x05, command);
		
		d = reply[2:]
		value = 0
		for n in range(8):#####  it was 8		
			value = value + (d[n] << (8*n))
		return value
		

        ##Opens the acquisition pipeline, allowing the data frames read from the shared memory block to be written to disk by a writing applications
        # @param fileName The name of the file containg the data written by aDAQ/writeRaw
	# @param enableTrigger Enables the hardware/software coincidence triggers
        # @param writer The desired outout file format. Default is "TOFPET", which is equivalent to "writeRaw"
	def openAcquisition(self, fileName, enableTrigger = False, writer = "TOFPET"):
		if self.__initOK == False:
			print "Called ATB::openAcquisition before ATB::initialize!"
			exit(1)

		writerModeDict = { "writeRaw" : 'T', "TOFPET" : 'T', "ENDOTOFPET" : 'E', "NULL" : 'N', 'RAW' : 'R' }
		if writer  not in writerModeDict.keys():
			print "ERROR: when calling ATB::openAcquisition(), writer must be ", ", ".join(writerModeDict.keys())
		
		m = writerModeDict[writer]

		# Set this values to zero to disable the software filter
		cWindow = 0

		# Write out the channel map and trigger map to provide to writeRaw
		self.__tempChannelMapFile = tempfile.NamedTemporaryFile(delete=True)
		for channelID, (region, xi, yi, x, y, z) in self.config.channelMap.items():
			self.__tempChannelMapFile.write("%d\t%d\t%d\t%d\t%f\t%f\t%f\t0\n" % (channelID, region, xi, yi, x, y, z))

		self.__tempChannelMapFile.flush()

		self.__tempTriggerMapFile = tempfile.NamedTemporaryFile(delete=True)
		for a, b in self.config.triggerZones:
			self.__tempTriggerMapFile.write("%d\t%d\n" % (a, b))

		self.__tempTriggerMapFile.flush()

		
		if not enableTrigger:
			# We don't want to filter this data for coincidences
			self.__disableCoincidenceTrigger()
		else:
			# Try to enable the hardware coincidence trigger
			if self.__enableCoincidenceTrigger() == 0:
				pass
			else:
				# Enable the software trigger as fallback
				cWindow = self.config.triggerCoincidenceWindow

		cmd = [ "aDAQ/writeRaw", 
			self.__getSharedMemoryName(), "%d" % self.__getSharedMemorySize(), \
				m, fileName,
				"%e" % cWindow, "%e" % self.config.triggerMinimumToT, \
				"%e" % self.config.triggerPreWindow, "%e" % self.config.triggerPostWindow, \
				self.__tempChannelMapFile.name, self.__tempTriggerMapFile.name \
			]
		self.__acquisitionPipe = Popen(cmd, bufsize=1, stdin=PIPE, stdout=PIPE, close_fds=True)

        ## Acquires data and decodes it, while writting through the acquisition pipeline 
        # @param step1 Tag to a given variable specific to this acquisition 
        # @param step2 Tag to a given variable specific to this acquisition
        # @param acquisitionTime Acquisition time in seconds 
	def acquire(self, step1, step2, acquisitionTime):
		#print "Python:: acquiring %f %f"  % (step1, step2)
		(pin, pout) = (self.__acquisitionPipe.stdin, self.__acquisitionPipe.stdout)
		nRequiredFrames = int(acquisitionTime / self.__frameLength)

		template1 = "@ffIIi"
		template2 = "@I"
		n1 = struct.calcsize(template1)
		n2 = struct.calcsize(template2)

		self.doSync()
		wrPointer, rdPointer = (0, 0)
		while wrPointer == rdPointer:
			wrPointer, rdPointer = self.__getDataFrameWriteReadPointer()
		
		bs = self.__dshm.getSizeInFrames()
		index = rdPointer % bs
		startFrame = self.__dshm.getFrameID(index)
		stopFrame = startFrame + nRequiredFrames

                t0 = time()
		nBlocks = 0
		currentFrame = startFrame
		nFrames = 0
		lastUpdateFrame = currentFrame
		while currentFrame < stopFrame:
			wrPointer, rdPointer = self.__getDataFrameWriteReadPointer()
			while wrPointer == rdPointer:
				wrPointer, rdPointer = self.__getDataFrameWriteReadPointer()

			nFramesInBlock = abs(wrPointer - rdPointer)
			if nFramesInBlock > bs:
				nFramesInBlock = 2*bs - nFramesInBlock

			# Do not feed more than bs/2 frame blocks to writeRaw in a single call
			# Because the entire frame block won't be freed until writeRaw is done, we can end up in a situation
			# where writeRaw owns all frames and daqd has no buffer space, even if writeRaw has already processed 
			# some/most of the frame block
			if nFramesInBlock > bs/2:
				wrPointer = (rdPointer + bs/2) % (2*bs)

			data = struct.pack(template1, step1, step2, wrPointer, rdPointer, 0)
			pin.write(data); pin.flush()
			
			data = pout.read(n2)
			rdPointer,  = struct.unpack(template2, data)

			index = (rdPointer + bs - 1) % bs
			currentFrame = self.__dshm.getFrameID(index)

			self.__setDataFrameReadPointer(rdPointer)

			nFrames = currentFrame - startFrame + 1
			nBlocks += 1
			if (currentFrame - lastUpdateFrame) * self.__frameLength > 0.1:
				t1 = time()
				stdout.write("Python:: Acquired %d frames in %4.1f seconds, corresponding to %4.1f seconds of data (delay = %4.1f)\r" % (nFrames, t1-t0, nFrames * self.__frameLength, (t1-t0) - nFrames * self.__frameLength))
				stdout.flush()
				lastUpdateFrame = currentFrame
		t1 = time()
		print "Python:: Acquired %d frames in %4.1f seconds, corresponding to %4.1f seconds of data (delay = %4.1f)" % (nFrames, time()-t0, nFrames * self.__frameLength, (t1-t0) - nFrames * self.__frameLength)

		data = struct.pack(template1, step1, step2, wrPointer, rdPointer, 1)
		pin.write(data); pin.flush()

		data = pout.read(n2)
		rdPointer,  = struct.unpack(template2, data)
		self.__setDataFrameReadPointer(rdPointer)

		return None

	## \internal
        def readConfigSTICv3(self, portID, slaveID):

		# Some padding
		#data = data + "\x00\x00\x00"
		data=bytearray()
		memAddr = 1
                for i in range(0, 146):
			msb = (memAddr >> 8) & 0xFF
			lsb = memAddr & 0xFF
			return_data=self.sendCommand(portID, slaveID, 0x00, bytearray([0x02, msb, lsb]))
			#print [hex(x) for x in return_data[2:6]]

			data = data+return_data[2:6];
			memAddr = memAddr + 1

		#Cut away the unused bits
		return data


        def __uploadConfigSTICv3(self, globalAsicID, asicConfig):
		data = asicConfig.data
		portID, slaveID, localAsicID = self.asicIDGlobalToTuple(globalAsicID);

		# Some padding
		data = data + "\x00\x00\x00"
		memAddr = 1
	
		while len(data) > 3:
			d = data[0:4]
			data = data[4:]
			msb = (memAddr >> 8) & 0xFF
			lsb = memAddr & 0xFF
			#print "RAM ADDR %4d, nBytes Written = %d, nBytes Left = %d" % (memAddr, len(d), len(data))
			#print [hex(ord(x)) for x in d]
			self.sendCommand(portID, slaveID, 0x00, bytearray([0x00, msb, lsb] + [ ord(x) for x in d ]))
			memAddr = memAddr + 1
		
		#print "Configuring"
		self.sendCommand(portID, slaveID, 0x00, bytearray([0x01,localAsicID]))
			
		

		return None


        ## Uploads the entire configuration (needs to be assigned to the abstract ATB.config data structure). It prints warning messages if active ASICs are not being properly configured.
        def uploadConfig(self):
		for portID, slaveID in self.__getPossibleFEBDs(): # Iterate on all possible FEB/D
			if (portID, slaveID) not in self.getActiveFEBDs():
				continue

			for localAsicID, globalAsicID in enumerate(self.getGlobalAsicIDsForFEBD(portID, slaveID)): # Iterate on all possible ASIC in a FEB/D
				asicType = self.__asicType[globalAsicID]
				ac = self.config.asicConfig[globalAsicID]
				asicOK = self.__activeAsics[globalAsicID]
				if ac == None and asicOK == False:
					continue
				elif ac == None and asicOK == True:
					print "WARNING: ASIC %d (P%02d S%02d A%02d) active but no config specified. Skipping" % (globalAsicID, portID, slaveID, localAsicID)
					continue
				elif ac != None and asicOK == False:
					print "WARNING: Configuration specified for non-active ASIC %d (P%02d S%02d A%02d). Skipping" % (globalAsicID, portID, slaveID, localAsicID)
					continue

				if asicType == 0x00010001:
					if not isinstance(ac, tofpet.AsicConfig):
						print "WARNING: Configuration type mismatch for ASIC %d (P%02d S%02d A%02d). Skipping" % (globalAsicID, portID, slaveID, localAsicID)
						continue
					self.__uploadConfigTOFPET(globalAsicID, ac)
				elif asicType == 0x00020003:
					if not isinstance(ac, sticv3.AsicConfig):
						print "WARNING: Configuration type mismatch for ASIC %d (P%02d S%02d A%02d). Skipping" % (globalAsicID, portID, slaveID, localAsicID)
						continue
					self.__uploadConfigSTICv3(globalAsicID, ac)
				else:
					raise ErrorInvalidAsicType(portID, slaveID, asicType)	
			
			for globalHVChannelID in self.getGlobalHVChannelIDForFEBD(portID, slaveID):
				hvValue = self.config.hvBias[globalHVChannelID]
				if hvValue is None:
					continue
				self.setHVDAC(globalHVChannelID, hvValue)
          
	def __uploadConfigTOFPET(self, asic, ac):
		#stdout.write("Configuring ASIC %3d " % asic); stdout.flush()
		# Force parameters!
		# Enable "veto" to reset TDC during configuration, to ensure more consistent results
		ac.globalConfig.setValue("veto_en", 1)
		for n, cc in enumerate(ac.channelConfig):
			# Enable deadtime to reduce insane events
			cc.setValue("deadtime", 3);


			 # Clamp thresholds to ensure we're in the valid range
			thresholdClamp = 15
			if cc.getValue("vth_T") < thresholdClamp:
				cc.setValue("vth_T", thresholdClamp)
			if cc.getValue("vth_E") < thresholdClamp:
				cc.setValue("vth_E", thresholdClamp) 

			self.doTOFPETAsicCommand(asic, "wrChCfg", channel=n, value=cc)

		for n, cc in enumerate(ac.channelTConfig):
			self.doTOFPETAsicCommand(asic, "wrChTCfg", channel=n, value=cc)
			#stdout.write("CH %2dT " %n);stdout.flush()
			

		portID, slaveID, lAsicID  = self.asicIDGlobalToTuple(asic)
		nLinks = self.readFEBDConfig(portID, slaveID, 0, 1);
		if nLinks == 1:
			ac.globalConfig.setValue("tx_mode", 0)
		elif nLinks == 2:
			ac.globalConfig.setValue("tx_mode", 1)
		else:
			raise ErrorInvalidLinks(portID, slaveID, nLinks)

		ac.globalConfig.setValue("ddr_mode", 1)

		self.doTOFPETAsicCommand(asic, "wrGlobalCfg", value=ac.globalConfig)
		self.doTOFPETAsicCommand(asic, "wrGlobalTCfg", value=ac.globalTConfig)
		#print "DONE!"

	## Initializes the temperature sensors in the FEB/As
	# Return the number of active sensors found in FEB/As
	def getNumberOfTemperatureSensors(self, portID, slaveID):
		asicType = self.readFEBDConfig(portID, slaveID, 0, 0)
		if asicType not in [0x00010001]:
			return 0

		din = [ 3, 0x55, 0b10001100, 0b10010000 ]
		din = bytearray(din)
		dout = self.sendCommand(portID, slaveID, 0x04, din)
		if len(dout) < 5:
			raise TMP104CommunicationError(portID, slaveID, din, dout)
		
		nSensors = dout[4] & 0x0F
	
		din = [ 3, 0x55, 0b11110010, 0b01100011]
		din = bytearray(din)
		dout = self.sendCommand(portID, slaveID, 0x04, din)
		if len(dout) < 5:
			raise TMP104CommunicationError(portID, slaveID, din, dout)

		din = [ 2 + nSensors, 0x55, 0b11110011 ]
		din = bytearray(din)
		dout = self.sendCommand(portID, slaveID, 0x04, din)
		if len(dout) < (4 + nSensors):
			raise TMP104CommunicationError(portID, slaveID, din, dout)

		return nSensors

	## Reads the temperature found in the specified FEB/D
	# @param portID  DAQ port ID where the FEB/D is connected
	# @param slaveID Slave ID on the FEB/D chain
	# @param nSensors Number of sensors to read
	def getTemperatureSensorReading(self, portID, slaveID, nSensors):
			din = [ 2 + nSensors, 0x55, 0b11110001 ]
			din = bytearray(din)
			dout = self.sendCommand(portID, slaveID, 0x04, din)
			if len(dout) < (4 + nSensors):
				raise TMP104CommunicationError(portID, slaveID, din, dout)

			temperatures = dout[4:]
			for i, t in enumerate(temperatures):
				if t > 127: t = t - 256
				temperatures[i] = t
			return temperatures

