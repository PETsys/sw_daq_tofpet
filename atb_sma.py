import atb

class ATB(atb.ATB):
        ## Sets the properties of the internal FPGA which feeds USER_GPIO_P/N
        # @param length Sets the length of the test pulse, from 0.25 to 1023.25 clock periods. 0 disables the test pulse.
        # @param interval Sets the interval between test pulses. The actual interval will be (interval+1)*1024 clock cycles. Ignored in single shot mode.
        # @param finePhase Defines the delay of the test pulse regarding the start of the frame, in units of 1/442 of the clock.
        # @param invert Sets the polarity of the test pulse: active low when ``True'' and active high when ``False''
        # @param singleShot Pulse is emmited only when a button is pressed
	def enableSMATestPulseGenerator(self, length, interval, finePhase, invert, singleShot=False):
		v = 0x0
		if length > 0:
			v |= (length & 0x3)
			v |= (((length>>2) + 1) & 0x3FF) << 4
			
		v |= (interval & 0x1FFFFF) << 16
		
		v |= (finePhase & 0xFFFFFF) << 37
		
		if invert:
			v |= (1 & 0x1) << 61
		
		if not singleShot:
			v |= (1 & 0x1) << 63
			
		print "%016x" % v
		
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 14, v)
	
        ## Disables test pulse
	def disableSMATestPulseGenerator(self):
		for portID, slaveID in self.getActiveFEBDs():
			self.writeFEBDConfig(portID, slaveID, 0, 14, 0x0)


		


		
	