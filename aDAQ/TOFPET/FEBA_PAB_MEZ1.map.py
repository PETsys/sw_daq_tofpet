# -*- coding: utf-8 -*-
from sys import argv
import re

def main(argv):
	inputFile = open(argv[1], "r")
	outputFile = open(argv[2], "w")

	regex = re.compile(r'([0-9]+)\s([0-9]+)\s([0-9]+)\s([0-9]+)\s([0-9\.]+)\s([0-9\.]+)\s([0-9\.]+)\s([0-9]+).*')
	for line in inputFile:
		m = regex.match(line)
		assert m is not None
		
		channel, region, xi, yi, x, y, z, hv = m.groups()
		channel = int(channel)
		asic = channel / 64
		channel = channel % 64
		asic = 1 - asic
		channel = 64 * asic + channel

		region = int(region)
		xi = int(xi)
		yi = int(yi)
		x = float(x)
		y = float(x)
		z = float(z)
		hv = int(hv)

		## Define trigger regions along the long axis of FEBA

		if xi < 8:
			region = 0
		else:
			region = 1

		# Map HV to PAB _SW_ DAC channels
		hv = hv + 16

		outputFile.write("%d\t%d\t%d\t%d\t%f\t%f\t%f\t%d\n" % (channel, region, xi, yi, x, y, z, hv))

	outputFile.close()
	inputFile.close()
		


if __name__ == '__main__':
	main(argv)
