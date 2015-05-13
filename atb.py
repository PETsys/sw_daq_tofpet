# -*- coding: utf-8 -*-

## @package atb
# This module defines all the classes, variables and methods to operate the TOFPET ASIC via a UNIX socket  

import crcmod
import serial
from math import log, ceil
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
from sys import maxint

import DSHM

import tofpet
import sticv3



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
		maxASIC = 64
		maxDAC = 8
                self.asicConfigFile = [ "Default Configuration" for x in range(maxASIC) ]
                self.asicBaselineFile = [ "None" for x in range(maxASIC) ]
                self.HVDACParamsFile = "None"
                self.asicConfig = [ None for x in range(maxASIC) ]
		self.hvBias = [ None for x in range(32*maxDAC) ]
		self.hvParam = [ (1.0, 0.0) for x in range(32*maxDAC) ]
		return None
        
        ## Writes formatted text file with all parameters and additional information regarding the state of the system  
        # @param prefix Prefix of the file name to be written (it will have the suffix .params)
        def writeParams(self, prefix):
          activeAsicsIDs = [ i for i, ac in enumerate(self.asicConfig) if isinstance(ac, tofpet.AsicConfig) ]
          minAsicID = min(activeAsicsIDs);
    
          defaultAsicConfig = AsicConfig()
          
          global_params= self.asicConfig[minAsicID].globalConfig.getKeys()
          channel_params= self.asicConfig[minAsicID].channelConfig[0].getKeys()
          

          f = open(prefix+'.params', 'w')
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
    
          check=True
          for i,key in enumerate(channel_params):
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
        
            f.write("\maxASIC%d.Global{\n"%ac_ind)  
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

	  for entry in self.hvBias:
            if entry!= None:
              f.write("%f"%entry)
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
class CommandErrorTimeout:
	def __init__(self, portID, slaveID):
		self.addr = portID, slaveID
	def __str__(self):
		return "Time out from FEB/D at port %2d, slave %2d" % self.addr
## Exception: The number of ASIC data links read from FEB/D is invalid.
#  Indicates a communication problem or bad firmware in FEB/D.
class ErrorInvalidLinks:
	def __init__(self, portID, slaveID, value):
		self.addr = value, portID, slaveID
	def __str__(self):
		return "Invalid NLinks value (%d) from FEB/D at port %2d, slave %2d" % self.addr
## Exception: the ASIC type ID read from FEB/D is invalid.
#  Indicates a communication problem or bad firmware in FEB/D.
class ErrorInvalidAsicType: 
	def __init__(self, portID, slaveID, value):
		self.addr = portID, slaveID, value
	def __str__(self):
		return "Invalid ASIC type FEB/D at port %2d, slave %2d: %016llx" % self.addr
## Exception: no active FEB/D was found in any port.
#  Indicates that no FEB/D is plugged and powered on or that it's not being able to establish a data link.
class ErrorNoFEB:
	def __str__(self):
		return "No active FEB/D on any port"

## Exception: testing for ASIC presence in FEB/D has returned a inconsistent result.
#  Indicates that there's a FEB/A board is not properly plugged or a hardware problem.
class ErrorAsicPresenceInconsistent:
	def __init__(self, portID, slaveID, asicID, s):
		self.__data = (portID, slaveID, asicID, s)
	def __str__(self):
		return "ASIC at port %2d, slave %2d, asic %2d has inconsistent state: %s" % (self.__data)

## Exception: testing for ASIC presence in FEB/D has changed state after system initialization.
#  Indicates that there's a FEB/A board is not properly plugged or a hardware problem.
class ErrorAsicPresenceChanged:
	def __init__(self, portID, slaveID, asicID):
		self.__data = (portID, slaveID, asicID)
	def __str__(self):
		return "ASIC at port %2d, slave %2d, asic %2d changed state" % (self.__data)
	


	
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
		self.__crcFunc = crcmod.mkCrcFun(0x104C11DB7, rev=False, initCrc=0x0A1CB27F)
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
		self.__activeAsics = [ False for x in range(64) ]
		self.__asicType = [ None for x in range(64) ]
		return None


        ## Starts the data acquisition
	# @param mode If set to 1, only data frames  with at least one event are transmitted. If set to 2, all data frames are transmitted. If set to 0, the data transmission is stopped
	def start(self, mode=2):
		nTry = 0
		while True:
			try:
				return self.__start(mode)
			except ErrorAsicPresenceChanged as e:
				nTry = nTry + 1;
				if nTry > 5: raise e
				print "WARNING: ", e, ", retrying..."

	def __start(self, mode=2):
		# First, generate a "sync" in the FEB/D (or ML605) itself
		for portID, slaveID in self.getActiveFEBDs():
			self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))

		#sleep(0.120) # Sleep at least 100 ms while the SYNC proceeds

		for portID, slaveID in self.getActiveFEBDs(): 
			self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)
			self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))

		# Now, start the DAQ!
		mode = 2 # Do not send mode 1 to daqd!
		template1 = "@HH"
		template2 = "@H"
		n = struct.calcsize(template1) + struct.calcsize(template2);
		data = struct.pack(template1, 0x01, n) + struct.pack(template2, mode)
		self.__socket.send(data)
		sleep(0.120) # Sleep at least 100 ms while the SYNC proceeds

		# Check the status from all the ASICs
		for portID, slaveID in self.getActiveFEBDs():
			nLocalAsics = len(self.getGlobalAsicIDsForFEBD(portID, slaveID))
			
			deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
			decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

			deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(nLocalAsics) ]
			decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(nLocalAsics) ]

			for i, globalAsicID in enumerate(self.getGlobalAsicIDsForFEBD(portID, slaveID)):
				localOK = deserializerStatus[i] and decoderStatus[i] 
				globalOK = self.__activeAsics[globalAsicID]
				if localOK != globalOK:
					raise ErrorAsicPresenceChanged(portID, slaveID, i)

		return None
       
        ## Stops the data acquisition, the same as calling start(mode=0)
	def stop(self):
		template1 = "@HH"
		template2 = "@H"
		n = struct.calcsize(template1) + struct.calcsize(template2);
		data = struct.pack(template1, 0x01, n) + struct.pack(template2, 0)
		self.__socket.send(data)
		return None

        ## Returns the size of the allocated memory block 
	def __getSharedMemorySize(self):
		return self.__dshm.getSize()
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
		template = "@HH"
		n = struct.calcsize(template)
		data = struct.pack(template, 0x06, n)
		self.__socket.send(data);

		template = "@HQ"
		n = struct.calcsize(template)
		data = self.__socket.recv(n);
		length, mask = struct.unpack(template, data)
		reply = [ n for n in range(64) if (mask & (1<<n)) != 0 ]
		return reply

	## Returns an array of (portID, slaveID) for the active FEB/Ds (PAB) 
	def getActiveFEBDs(self):
		return [ (x, 0) for x in self.getActivePorts() ]

	def __getPossibleFEBDs(self):
		return [ (x, 0) for x in range(4) ]

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
	# @param port The port for which to get the desired output 
	def getFEBDCount1(self, portID, slaveID):
		tx = self.readFEBDConfig(portID, slaveID, 1, 0)
		rx = self.readFEBDConfig(portID, slaveID, 1, 1)
		rxBad = self.readFEBDConfig(portID, slaveID, 1, 2)
	
		
		tx = binToInt(grayToBin(intToBin(tx, 48)))
		rx = binToInt(grayToBin(intToBin(rx, 48)))
		rxBad = binToInt(grayToBin(intToBin(rxBad, 48)))
		return (tx, rx, rxBad)


		
        ## Returns a data frame read form the shared memory block
	def getDataFrame(self, nonEmpty=False):
		index = self.__getDataFrameByIndex(nonEmpty = nonEmpty)
		if index is None:
			return None

	
		frameID = self.__dshm.getFrameID(index)
		frameLost = self.__dshm.getFrameLost(index)
		nEvents = self.__dshm.getNEvents(index)

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

		reply = { "id" : frameID, "lost" : frameLost, "events" : events }
		self.__returnDataFrameByIndex(index)
		return reply

	def __getDataFrameByIndex(self, nonEmpty = False):
		rawIndexes = self.__getDataFramesByRawIndex(1, nonEmpty=nonEmpty);
		if len(rawIndexes) == 0:
			return None
		template = "@i"
		index, = struct.unpack(template, rawIndexes)
		return index
	
	def __returnDataFrameByIndex(self, index):
		template = "@i"
		rawIndexes = struct.pack(template, index)
		self.__returnDataFramesByRawIndex(rawIndexes)


	def __returnDataFramesByRawIndex(self, rawIndexes):
		template = "@i"
		n0 = struct.calcsize(template)
		nFrames = len(rawIndexes)/n0
		template1 = "@HH";
		template2 = "@H";
		template3 = "@i"
		n1 = struct.calcsize(template1)
		n2 = struct.calcsize(template2)
		n3 = struct.calcsize(template3) * nFrames
		data = struct.pack(template1, 0x04, n1+n2+n3) + struct.pack(template2, nFrames) + rawIndexes[0:n3]
		self.__socket.send(data)

		  
	def __getDataFramesByRawIndex(self, nRequestedFrames, nonEmpty = False):
		template1 = "@HH";
		template2 = "@HH";
		if nonEmpty: 
			p2 = 1	
		else:
			p2 = 0

		n1 = struct.calcsize(template1)
		n2 = struct.calcsize(template2)
		data = struct.pack(template1, 0x03, n1+n2) + struct.pack(template2, nRequestedFrames, p2)
		self.__socket.send(data)

		template = "@HH"
		n = struct.calcsize(template)
		data = self.__socket.recv(n)
		length, nFrames = struct.unpack(template, data)

		#print "DBG %d %d %d" % (nRequestedFrames, nTry, nFrames)
		if nFrames == 0:
			return ""
				
		template = "@i"
		n = struct.calcsize(template)
		return self.__socket.recv(n * nFrames)
	

	
       	## Sends a command to the FPGA
        # @param portID
        # @param slaveID
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

		febID = asicID / 16
		reply = self.sendCommand(febID, 0, 0x00, cmd)
		status = reply[1]

		if status == 0xE3:
			raise tofpet.ConfigurationErrorBadAck(febID, 0, asicID % 16, 0)
		elif status == 0xE4:
			raise tofpet.ConfigurationErrorBadCRC(febID, 0, asicID % 16)
		elif status == 0xE5:
			raise tofpet.ConfigurationErrorBadAck(febID, 0, asicID % 16, 1)
		elif status != 0x00:
			raise tofpet.ConfigurationErrorGeneric(febID, 0, asicID % 16, status)

		if isRead:
			#print [ "%02X" % x for x in reply ]
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
				raise tofpet.ConfigurationErrorBadRead(febID, 0, asicID % 16, value, readValue)

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
	# @param portID  DAQ port number where the FEB/D is connected
	# @param slaveID Slave number on the FEB/D chain
	def getGlobalAsicIDsForFEBD(self, portID, slaveID):
		return [x for x in range(16*portID, 16*portID + 16)]
	
	## Returns a tuple with the (portID, slaveID, localAsicID) for which an globalAsicID belongs
	# @param asicID Global ASIC ID
	def asicIDGlobalToTuple(self, asicID):
		return (asicID / 16, 0, asicID % 16)

	
	def __initializeFEBD_TOFPET(self, portID, slaveID):
		print "INFO: FEB/D at port %d, slave %d is of type TOFPET" % (portID, slaveID)
		# Disable data reception logic
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0x0)
		# Disable test pulse
		self.setTestPulseNone()
		# Send a 64K clock long reset
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0xFF, 0xFF]))
		sleep(0.120 + 64*self.__frameLength)	# Need to wait for at least 100 ms, plus the reset time
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
			except tofpet.ConfigurationError as e:
				pass

		# Enable the reception logic	
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)
		#self.sendCommand(portID, slaveID, 0x03, bytearray([0x04, 0x00, 0x00]))
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))
		sleep(0.120)	# Need to wait for at least 100 ms, plus the reset time

		deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
		decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

		deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		
		localAsicActive = [ False for x in localAsicIDList ]
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
		data = allOff.data
		data = data + "\x00\x00\x00"
		memAddr = 1
		while len(data) > 3:
			d = data[0:4]
			data = data[4:]
			msb = (memAddr >> 8) & 0xFF
			lsb = memAddr & 0xFF
			#print "RAM ADDR %4d, nBytes Written = %d, nBytes Left = %d" % (memAddr, len(d), len(data))
			#print [hex(ord(x)) for x in d]
			self.sendCommand(febID, 0, 0x00, bytearray([0x00, msb, lsb] + [ ord(x) for x in d ]))
			memAddr = memAddr + 1	

		for n in range(8):
			ldoVector = self.readFEBDConfig(portID, slaveID, 0, 5)
			if ldoVector & (1<<n) == 0: # This LDO is OFF, let's turn it on	
				print "INFO: LDO %d was OFF, turning ON"
				ldoVector = ldoVector | (1<<n)
				self.writeFEBDConfig(portID, slaveID, 0, 5, ldoVector)
				for i in range(16): # Apply the ALL_OFF configuration to all chips in FEB/D
					self.sendCommand(portID, slaveID, 0x00, bytearray([0x01, i]))

		localAsicConfigOK = [ True for x in localAsicIDList ] # For STICv3, we don't check the configuration status

		# Enable the reception logic	
		self.writeFEBDConfig(portID, slaveID, 0, 4, 0xF)
		#self.sendCommand(portID, slaveID, 0x03, bytearray([0x04, 0x00, 0x00]))
		self.sendCommand(portID, slaveID, 0x03, bytearray([0x00, 0x00, 0x00, 0x00, 0x00]))
		sleep(0.120)	# Need to wait for at least 100 ms, plus the reset time

		deserializerStatus = self.readFEBDConfig(portID, slaveID, 0, 2)
		decoderStatus = self.readFEBDConfig(portID, slaveID, 0, 3)

		deserializerStatus = [ deserializerStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		decoderStatus = [ decoderStatus & (1<<n) != 0 for n in range(len(localAsicConfigOK)) ]
		
		localAsicActive = [ False for x in localAsicIDList ]
		for i, asicID in enumerate(localAsicIDList):
			self.__asicType[asicID] = 0x00020003
			triplet = (localAsicConfigOK[i], deserializerStatus[i], decoderStatus[i])
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

	## Sends the entire configuration (needs to be assigned to the abstract ATB.config data structure) to the ASIC and starts to write data to the shared memory block
        # @param maxTries The maximum number of attempts to read a valid dataframe after uploading configuration 
	def initialize(self, maxTries = 1):
		assert self.config is not None
		activePorts = self.getActivePorts()
		print "INFO: active FEB/D on ports: ", (", ").join([str(x) for x in activePorts])
		# Stop acquisition (makes it easier to send commands)
		self.stop()		
		sleep(0.5)

		for portID, slaveID in self.getActiveFEBDs():
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

			indexes = self.__getDataFramesByRawIndex(128)
			self.__returnDataFramesByRawIndex(indexes)
		

		return
	  

	## Returns a (portID, slaveID, localID) tuple for a globalHVChannel
	# @param globalHVChannelID Global HV channel ID
	def hvChannelGlobalToTulple(self, globalHVChannelID):
		return (globalHVChannelID / 64, 0, globalHVChannelID % 64)

	## Return a list of global HV channel IDs for a FEB/D
	# @param portID 
	# @param slaveID
	def getGlobalHVChannelIDForFEBD(self, portID, slaveID):
		return [ x for x in range(portID * 64, portID * 64 + 64) ]

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
	
        ## Disables test pulse 
	def setTestPulseNone(self):
		cmd =  bytearray([0x01] + [0x00 for x in range(8)])
		for portID, slaveID in self.getActiveFEBDs():
			self.sendCommand(portID, slaveID, 0x03,cmd)
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
	# @param portID
	# @param slaveID
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
	# @param portID
	# @param slaveID
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
        # @param cWindow Coincidence window. If different from 0, only events with a time of arrival difference of cWindow (in seconds) will be written to disk. 
        # @param writer The name of the application to be used to save data to disk (Typically writeRaw). 
	def openAcquisition(self, fileName, cWindow, writer=None):
		if writer not in ["writeRaw", "writeRawE"]:
			print "ERROR: when calling ATB::openAcquisition(), writer must be set with either:"
			print " writeRaw	-- standard TOFPET RAW data format"
			print " writeRawE	-- EndTOFPET-US RAW data format"


		from os import environ
		if not environ.has_key('ADAQ_CRYSTAL_MAP'):
			print 'Error: ADAQ_CRYSTAL_MAP environment variable is not set'
			exit(1)

		cmd = [ "aDAQ/%s" % writer, self.__getSharedMemoryName(), "%d" % self.__getSharedMemorySize(), \
				"%e" % cWindow, \
				fileName ]
		self.__acquisitionPipe = Popen(cmd, bufsize=1, stdin=PIPE, stdout=PIPE, close_fds=True)

        ## Acquires data and decodes it, while writting through the acquisition pipeline 
        # @param step1 Tag to a given variable specific to this acquisition 
        # @param step2 Tag to a given variable specific to this acquisition
        # @param acquisitionTime Acquisition time in seconds 
	def acquire(self, step1, step2, acquisitionTime):
		#print "Python:: acquiring %f %f"  % (step1, step2)
		(pin, pout) = (self.__acquisitionPipe.stdin, self.__acquisitionPipe.stdout)
		nFrames = 0

		template1 = "@ffii"
		template2 = "@i"
		n1 = struct.calcsize(template1)
		n2 = struct.calcsize(template2)
		rawIndexes = self.__getDataFramesByRawIndex(1024)

                nRequiredFrames = acquisitionTime / self.__frameLength
		t0 = time()
		while nFrames < nRequiredFrames:
			nFramesInBlock = len(rawIndexes)/n2
			if nFramesInBlock <= 0:
				print "Python:: Could not read any data frame indexes"
				break
			#print "Python:: About to push %d frames" % nFramesInBlock
			
			header = struct.pack(template1, step1, step2, nFramesInBlock, 0)
			pin.write(header)
			pin.write(rawIndexes[0:n2*nFramesInBlock])
			pin.flush()

			nFrames += nFramesInBlock
			newRawIndexes = self.__getDataFramesByRawIndex(1024)

			tmp = pout.read(n2*nFramesInBlock)
			#print "Python:: got back %d frames" % (len(tmp)/n2)
			
			self.__returnDataFramesByRawIndex(rawIndexes)
			rawIndexes = newRawIndexes

		#print "Python:: Returning last frames"
		self.__returnDataFramesByRawIndex(rawIndexes)


		#t0 = time()
		#while time() - t0 < acquisitionTime:
			#print "Asking for indexes..."
			#rawIndexes = self.__getDataFramesByRawIndex(1024)
			
			#self.__returnDataFramesByRawIndex(rawIndexes)

		# Close the deal by sending a block with a -1 index and endOfStep set to 1
		header = struct.pack(template1, step1, step2, 1, 1)
		rawIndexes = struct.pack(template2, -1)		
		#print "Python:: closing step with ",[hex(ord(c)) for c in rawIndexes ]
		pin.write(header)
		pin.write(rawIndexes)
		pin.flush()
		rawIndexes = pout.read(n2)
		index, = struct.unpack(template2, rawIndexes)
		assert index == -1
		#print "Python:: got back %ld\n" % (long(index)), [hex(ord(c)) for c in rawIndexes ]
		#self.__returnDataFramesByRawIndex(rawIndexes)
	

		print "Python:: Acquired %d frames in %f seconds, corresponding to %f seconds of data" % (nFrames, time()-t0, nFrames * self.__frameLength)
		return None

	## \internal
        def readConfigSTICv3(self):

		# Some padding
		#data = data + "\x00\x00\x00"
		data=bytearray()
		memAddr = 1
                for i in range(0, 146):
			msb = (memAddr >> 8) & 0xFF
			lsb = memAddr & 0xFF
			return_data=self.sendCommand(0, 0, 0x00, bytearray([0x02, msb, lsb]))
			#print [hex(x) for x in return_data[2:6]]

			data = data+return_data[2:6];
			memAddr = memAddr + 1

		#Cut away the unused bits
		return data


        def __uploadConfigSTICv3(self, globalAsicID, asicConfig):
		data = asicConfig.data
		porID, slaveID, localAsicID = self.asicIDGlobalToTuple(globalAsicID);

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
		self.sendCommand(portID, slaveID, 0, 0x00, bytearray([0x01,localAsicID]))
			
		

		return None


        ## Uploads the entire configuration (needs to be assigned to the abstract ATB.config data structure). It prints warning messages if active ASICs are not being properly configured.
        def uploadConfig(self):
		for portID, slaveID in self.__getPossibleFEBDs(): # Iterate on all possible FEB/D
			if (portID, slaveID) not in self.getActiveFEBDs():
				continue

			asicType = self.readFEBDConfig(portID, slaveID, 0, 0)
			for localAsicID, globalAsicID in enumerate(self.getGlobalAsicIDsForFEBD(portID, slaveID)): # Iterate on all possible ASIC in a FEB/D
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
		for n, cc in enumerate(ac.channelConfig):
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
			

		portID = asic / 16
		slaveID = 0
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


		
