# -*- coding: utf-8 -*-
#
# File: stic_config_interface.py
#
# Python commandline user interface to change STIC ASIC configuration on the fly
# The individual parameters can be accessed for each channel with shorter names
# The new configuration can be uploaded to a specified ASIC with a single command
# 
#
# Author: Tobias Harion
# Tue Nov 18 16:58:45 CET 2014

import sticv3
import stic_pythonconfig
import atb



class config_interface:

    def __init__(self,data_file, uut=None):
        self.conf=sticv3.AsicConfig(data_file)
        self.channel=0
        self.ch_par=stic_pythonconfig.channel_params
        self.tdc_par=stic_pythonconfig.tdc_params
        self.gen_par=stic_pythonconfig.gen_params


        #DEFINE THE CONNECTION TO THE daqd software
        self.uut = uut
	if self.uut is not None:
	        self.uut.stop()

    #Show available parameters for the STiC channels
    def show_chpar(self):
        for i in self.ch_par.iterkeys():
            print i

    #Show available parameters for the STiC TDC
    def show_tdcpar(self):
        for i in self.tdc_par.iterkeys():
            print i

    def show_genpar(self):
        for i in self.gen_par.iterkeys():
            print i

    def set_chpar(self,par,chan,value):
        par_name=self.ch_par[par]+str(chan)
        self.conf.config_dict[par_name]=hex(value)[2:]

    #Get back the value of the channel
    def get_chpar(self,par,chan):
        par_name=self.ch_par[par]+str(chan)
        return int(self.conf.config_dict[par_name],16);

    def set_tdcpar(self,par,side,value):
        par_name=self.tdc_par[par].replace("SIDE",side)
        self.conf.config_dict[par_name]=hex(value)[2:]

    def set_genpar(self,par,value):
        par_name=self.gen_par[par]
        self.conf.config_dict[par_name]=hex(value)[2:]

    def mask_allch(self):
        for i in range(0,64):
            self.set_chpar("mask",i,1)

    def unmask_allch(self):
        for i in range(0,64):
            self.set_chpar("mask",i,0)

    def query_masked_ch(self):
        for i in range(0,64):
            par_name=self.ch_par["mask"]+str(i)
            if self.conf.config_dict[par_name]=='1':
                print "Channel " + str(i) + "masked"

    #Upload the new configuration to the specified STiC ASIC
    def updateAsic(self,asic_num):
        print "Updating Configuration of asic %d" % asic_num
        self.conf.gen_config_pattern()
        self.uut.config.asicConfig[asic_num]=self.conf
        self.uut.uploadConfigSTICv3(asic_num, self.uut.config.asicConfig[asic_num])
        self.uut.sendCommand(1,0x00, bytearray([0x01, asic_num]))
        ret_data = self.uut.readConfigSTICv3()

        #For manual checking of the configuration uncomment the following section 
        print "Returned configuration:"
        print [hex(x) for x in ret_data[0:582]]
        print "Original configuration:"
        self.uut.config.asicConfig[asic_num].print_config()
        #End of manual config checking 

        self.uut.config.asicConfig[asic_num].compare_config(ret_data)


    def resetAsics(self):
        print "Sending reset"
        self.uut.sendCommand(1,0x03, bytearray([0x00, 0x00, 0x00, 0xFF, 0xFF]))



    def turn_on_asic(self,asicID):
        #Calculate the new power enable pattern and update the FPGA 
        self.asic_powermask=0xFF
        self.asic_powermask=self.asic_powermask & 2**asicID
        self.uut.sendCommand(1,0x03, bytearray([0x04, 0x01, self.asic_powermask]))

    def turn_alloff(self):
        self.asic_powermask=0x00;
        self.uut.sendCommand(1,0x03, bytearray([0x04, 0x01, 0x00]))
