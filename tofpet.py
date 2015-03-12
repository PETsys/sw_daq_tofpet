# -*- coding: utf-8 -*-
from bitarray import bitarray

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

## Contains parameters and methods related to the global operation of one ASIC. 
class AsicGlobalConfig(bitarray):
  ## Constructor. 
  # Defines and sets all fields to default values. Most important fields are:
  # 
  def __init__(self, initial=None, endian="big"):
	super(AsicGlobalConfig, self).__init__()

	if initial is not None:
		self[0:110] = bitarray(initial)
		return None

	self.__fields = {
	  'tdc_comp'			: [ 26 + 83 - x for x in range(6) ],
	  'comp_vcas'			: [ 26 + 77 - x for x in range(6) ],
	  'tdc_iref'			: [ 26 + 71 - x for x in range(6) ],
	  'tdc_tac'			: [ 26 + 65 - x for x in range(6) ],
	  'lvlshft'			: [ 26 + 54 + x for x in range(6) ],
	  'vib2'			: [ 26 + 48 + x for x in range(6) ],
	  'vib1' 			: [ 26 + 42 + x for x in range(6) ],
	  'sipm_idac_range' 		: [ 26 + 36 + x for x in range(6) ],
	  'sipm_idac_dcstart'		: [ 26 + 30 + x for x in range(6) ],
	  'disc_idac_range'		: [ 26 + 29 - x for x in range(6) ],
	  'disc_idac_dcstart'		: [ 26 + 23 - x for x in range(6) ],
	  'postamp'			: [ 26 + 17 - x for x in range(6) ],
	  'fe_disc_ibias'		: [ 26 + 11 - x for x in range(6) ],
	  'fe_disc_vcas'		: [ 26 + 05 - x for x in range(6) ],
	  'clk_o_en'			: [ 25 ],
	  'test_pattern' 		: [ 24 ],
	  'veto_en'			: [ 23 ],
	  'data_mode'			: [ 22 ],
	  'count_intv'			: [ 21, 20, 19, 18 ],
	  'count_trig_err'		: [ 17 ],
	  'fc_kf'			: [ 16 - x for x in range(8) ],
	  'fc_saturate'			: [ 8 ],
	  'tac_refresh'			: [ 7, 6, 5, 4 ],
	  'test_pulse_en'		: [ 3 ],
	  'ddr_mode' 			: [ 2 ],
	  'tx_mode'			: [ 1, 0 ]
	}



	self[0:110] = bitarray(110)

	# Reset values
	self.setValue('tdc_comp', 32)
	self.setValue('comp_vcas', 32)
	self.setValue('tdc_iref', 32)
	self.setValue('tdc_tac', 32)
	self.setValue('lvlshft', 44)
	self.setValue('vib1', 48)
	self.setValue('vib2', 48)
	self.setValue('sipm_idac_range', 40)
	self.setValue('sipm_idac_dcstart', 45)
	self.setValue('disc_idac_range', 48)
	self.setValue('disc_idac_dcstart', 35)
	self.setValue('postamp', 44)
	self.setValue('fe_disc_ibias', 43)
	self.setValue('fe_disc_vcas', 42)
	self.setValue('clk_o_en', 1)
	self.setValue('test_pattern', 0)
	self.setValue('veto_en', 0)
	self.setValue('data_mode', 1)
	self.setValue('count_intv', 0)
	self.setValue('count_trig_err', 0)
	self.setValue('fc_kf', 0)
	self.setValue('fc_saturate', 0)
	self.setValue('tac_refresh', 3)
	self.setValue('test_pulse_en', 1)
	self.setValue('ddr_mode', 0)
	self.setValue('tx_mode', 1)
	
	# Sane defaults
	self.setValue('sipm_idac_dcstart', 52)
	self.setValue('tac_refresh', 0)
	self.setValue('disc_idac_dcstart', 45);
	self.setValue('test_pulse_en', 0);
	self.setValue('tx_mode', 0)
	self.setValue("fe_disc_ibias", 20)
	self.setValue("count_intv", 15)
	
	# Old slihgtly insane settings
	#self[0:110] = bitarray(\
	  #'100000' + \
	  #'100000' + \
	  #'100000' + \
	  #'100000' + \
	  #'001101'[::-1] + \
	  #'000011'[::-1] + \
	  #'000011'[::-1] + \
	  #'000101'[::-1] + \
	  #'101101'[::-1] + \
	  #'110000' + \
	  #'100011' + \
	  #'101100' + \
	  #'101011' + \
	  #'100000' + \
	  #'10010000000000000000000000')

	# From testing
	self.setValue("tdc_iref", 42)		# was calibration with 48 ???
	self.setValue("tdc_iref", 32)		# 160 MHz
	self.setValue("disc_idac_dcstart", 45);
	self.setValue("disc_idac_dcstart", 50);
	self.setValue("disc_idac_range", 32);	#tryout
	self.setValue("vib1", 0)
	self.setValue("vib2", 20)
	self.setValue("sipm_idac_range", 32)
#	self.setValue("sipm_idac_dcstart", 46) # ATB
	self.setValue("sipm_idac_dcstart", 38) # TTB 
	self.setValue("tac_refresh", 0)
	self.setValue("lvlshft", 10)
#	self.setValue("sipm_idac_dcstart", 48) # TTB? Xtreme - preamp estrangulado!!! (strangled)
#	self.setValue("sipm_idac_dcstart", 42) # TTB? - começa a estrangular - starts to strangle!
#	self.setValue("sipm_idac_dcstart", 40) # TTB? OK: Vin = 435mV @vbl=63
	self.setValue("postamp", 50)
	#self.setValue("postamp", 40)

	# Introduced after oscillation problems were found
	self.setValue("vib2", 20)
	self.setValue("vib1", 24)
	self.setValue("sipm_idac_dcstart", 48)

  ## Set the value of a given parameter as an integer
  # @param key  String with the name of the parameter to be set
  # @param value  Integer corresponding to the value to be set	
  def setValue(self, key, value):
	b = intToBin(value, len(self.__fields[key]))
	self.setBits(key, b)
 
  ## Set the value of a given parameter as a bitarray
  # @param key  String with the name of the parameter to be set
  # @param value  Bitarray corresponding to the value to be set		
  def setBits(self, key, value):
	index = self.__fields[key]
	assert len(value) == len(index)
	for a,b in enumerate(index):
	  self[109 - b] = value[a]

  ## Returns the value of a given parameter as a bitarray
  # @param key  String with the name of the parameter to be returned	
  def getBits(self, key):
	index = self.__fields[key]
	value = bitarray(len(index))
	for a,b in enumerate(index):
	  value[a] = self[109 - b]
	return value
  
  ## Returns the value of a given parameter as an integer
  # @param key  String with the name of the parameter to be returned	
  def getValue(self, key):
	return binToInt(self.getBits(key))
 
  ## Prints the content of all parameters as a bitarray	
  def printAllBits(self):
	for key in self.__fields.keys():
	  print key, " : ", self.getBits(key)
  
  ## Prints the content of all parameters as integers
  def printAllValues(self):
	for key in self.__fields.keys():
	  print key, " : ", self.getValue(key)

  ## Returns all the keys (variables) in this class
  def getKeys(self):
	return self.__fields.keys()

## Contains parameters and methods related to the operation of one channel of the ASIC. 
class AsicChannelConfig(bitarray):
  ## Constructor
  # Defines and sets all fields to default values. Most important fields are:
  # 
  def __init__(self, initial=None, endian="big"):
	super(AsicChannelConfig, self).__init__()

	self.__baseline = 0

	if initial is not None:
		self[0:53] = bitarray(initial)
		return None
	
	self.__fields = {
	  'd_E'	: [48 + x for x in range(5) ],
	  'test' : [ 47, 46, 45, 44, 42 ],
	  'sync' : [ 43 ],
	  'deadtime' : [ 41, 40 ],
	  'meta_cfg' : [ 39, 38 ],
	  'latch_cfg' : [ 37, 36, 30 ],
	  'cgate_cfg' : [29, 28, 27 ],
	  'd_T' : [ 35 - x for x in range(5) ],
	  'g0' : [26, 25], 
	  'sh' : [ 24, 23 ],
	  'vth_E' : [17 + x for x in range(6) ],
	  'vth_T' : [11 + x for x in range(6) ],
	  'vbl' : [ 3, 2, 7, 8, 9, 10 ],
	  'inpol' : [ 6 ],
	  'fe_test_mode' : [ 5 ],
	  'fe_tmjitter' :  [ 4 ],
	  'praedictio' : [ 1 ],
	  'praedictio_delay' : [ 30, 29, 28, 27 ],
	  'enable' : [ 0 ]
	}
	
	self[0:53] = bitarray(53)
	
	# Reset Value
	self.setValue('d_E', 28)
	self.setBits('test', bitarray("00000"))
	self.setValue('sync', 1)
	self.setBits('deadtime', bitarray("00"))
	self.setBits('meta_cfg', bitarray("01"))
	self.setValue('latch_cfg', 1)
	self.setValue('cgate_cfg', 7)
	self.setValue('d_T', 7)
	self.setValue('g0', 3)
	self.setValue('sh', 3)
	self.setValue('vth_E', 3)
	self.setValue('vth_T', 59)
	self.setValue('vbl', 31)
	self.setValue('inpol', 1)
	self.setValue('fe_test_mode', 0)
	self.setValue('fe_tmjitter', 0)
	self.setValue('praedictio', 1)
	self.setValue('enable', 1)
	
	# Sane defaults
	self.setValue('d_E', 7)
	self.setValue('vbl', 48)
	self.setValue('sh', 3)
	self.setValue('latch_cfg', 0)
	self.setValue('cgate_cfg', 0)

	# Old slihgtly insane settings
	#self[0:53] = bitarray('11100000010000100001111111110111000011011111111000111')

	# From testing
	self.setValue("inpol", 0)
	self.setValue("d_T", 7) # x128 interpolation
	self.setValue("d_E", 7)
	#channelConfig.setValue("d_T", 7+16) # x256 interpolation
	#channelConfig.setValue("d_E", 7+16)
	#channelConfig.setValue("sh", 0)
	self.setValue("g0", 3)
	self.setValue("vbl", 52)
	self.setValue("vth_T", 5);
	self.setValue("vth_E", 5);

	# Introduced after oscillation problems were found
	self.setValue("vbl", 44)

  ## Set the value of a given parameter as an integer
  # @param key  String with the name of the parameter to be set
  # @param value  Integer corresponding to the value to be set		
  def setValue(self, key, value):
	b = intToBin(value, len(self.__fields[key]))
	self.setBits(key, b)

  ## Set the value of a given parameter as a bitarray
  # @param key  String with the name of the parameter to be set
  # @param value  Bitarray corresponding to the value to be set		
  def setBits(self, key, value):
	index = self.__fields[key]
	assert len(value) == len(index)
	for a,b in enumerate(index):
	  self[52 - b] = value[a]
  
  ## Returns the value of a given parameter as a bitarray
  # @param key  String with the name of the parameter to be returned	
  def getBits(self, key):
	index = self.__fields[key]
	value = bitarray(len(index))
	for a,b in enumerate(index):
	  value[a] = self[52 - b]
	return value
  
  ## Returns the value of a given parameter as an integer
  # @param key  String with the name of the parameter to be returned	
  def getValue(self, key):
	return binToInt(self.getBits(key))
  
  # Prints the content of all parameters as a bitarray		
  def printAllBits(self):
	for key in self.__fields.keys():
	  print key, " : ", self.getBits(key)
	  
  ## Prints the content of all parameters as integers
  def printAllValues(self):
	for key in self.__fields.keys():
	  print key, " : ", self.getValue(key)
 
  ## Set the baseline value in units of ADC (63 to 0)
  def setBaseline(self, v):
	self.__baseline = v

  ## Returns the baseline value for this channel
  def getBaseline(self):
	return self.__baseline
 
  ## Returns all the keys (variables) in this class
  def getKeys(self):
	return self.__fields.keys()

## A class containing instances of AsicGlobalConfig and AsicChannelConfig
#, as well as 2 other bitarrays related to test pulse configuration. Is related to one given ASIC.
class AsicConfig:
	def __init__(self):
		self.channelConfig = [ AsicChannelConfig() for x in range(64) ]
		self.channelTConfig = [ bitarray('0') for x in range(64) ]
		self.globalConfig = AsicGlobalConfig()
		self.globalTConfig = bitarray('1111110')	
		return None



class ConfigurationError:
	pass

class ConfigurationErrorBadAck(ConfigurationError):
	def __init__(self, portID, slaveID, asicID, value):
		self.addr = (value, portID, slaveID, asicID)
		self.errType = value
	def __str__(self):
		return "Bad ACK (%d) when configuring ASIC at port %2d, slave %2d, asic %2d"  % self.addr

class ConfigurationErrorBadCRC(ConfigurationError):
	def __init__(self, portID, slaveID, asicID):
		self.addr = (portID, slaveID, asicID)
	def __str__(self):
		return "Received configuration datta with bad CRC from ASIC at port %2d, slave %2d, asic %2d" % self.addr

class ConfigurationErrorStuckHigh(ConfigurationError):
	def __init__(self, portID, slaveID, asicID):
		self.addr = (portID, slaveID, asicID)
	def __str__(self):
		return "MOSI stuck high from ASIC at port %2d, slave %2d, asic %2d" % self.addr

class ConfigurationErrorGeneric(ConfigurationError):
	def __init__(self, portID, slaveID, asicID, value):
		self.addr = (value, portID, slaveID, asicID)
	def __str__(self):
		return "Unexpected configuration error %02X from ASIC at port %2d, slave %2d, asic %2d" % self.addr

