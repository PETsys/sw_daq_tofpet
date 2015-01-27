# -*- coding: utf-8 -*-
#
# File: sticv3_config.py
#
# Definition of the AsicConfig class for STiCv3 configuration files
# The configuration of a ASCII text file containing parameter names and values
# is read and the corresponding bitpattern is generated. This bitpattern can then
# be uploaded by the atb class.
#
# Author: Tobias Harion
# Tue Nov 18 16:58:45 CET 2014


from stic_pythonconfig import sticconf_dict
from bitarray import bitarray

class AsicConfig:
	def __init__(self,datafile):
		self.config_dict={}
		self.config_bin={}
		self.read_file(datafile)
		self.gen_config_pattern()


	#Read the stic configuration from the configfile
	def read_file(self, infile):
		f = open(infile,'r')
		for line in f:
			#The format of the config file is "PARAMETERNAME = HEXVALUE"
			config_array=line.split( )
			self.config_dict[config_array[0]]=config_array[2]
		f.close()



	def write_file(self, outfile):
		f = open(outfile,'w')

		for i in sticconf_dict:
                    outline=sticconf_dict[i][0] + " = " + self.config_dict[sticconf_dict[i][0]] + "\n"
                    #The format of the config file is "PARAMETERNAME = HEXVALUE"
                    f.write(outline);

		f.close()

	#convert the hexvalue to a bitarray
	#Warning: the function does not reduce
	def hexToBin(self, hexin, width):
		scale=16
		return bitarray(bin(int(hexin, scale))[2:].zfill(width))[0:width]


	def gen_config_pattern(self):
		a=bitarray(endian="little")
		for i in sticconf_dict:
			current_pattern=self.hexToBin(self.config_dict[sticconf_dict[i][0]], sticconf_dict[i][1])

			if(sticconf_dict[i][2]==1):
				a=a+current_pattern
			else:
				current_pattern.reverse()
				a=a+current_pattern

		self.data=a.tobytes()


	#Compare the supplied pattern with the stored configuration pattern
	def compare_config(self, compare_pattern):

		local_config=bytearray(self.data)
		local_config=local_config[0:582]
		local_compare=compare_pattern[0:582]

		#cut away the unused bits
		local_config[581]=local_config[581]&0x01
		local_compare[581]=local_compare[581]&0x01

		if local_config==local_compare:
			print "Patterns match"
			return True
		else:
			print "Patterns don't match"
			return False

	def print_config(self):
		local_config=bytearray(self.data)
		print len(local_config)
		print [hex(i) for i in local_config]
