# -*- coding: utf-8 -*-
from sys import argv
import re

# FEB/A to PAB adapter HV mapping
hvMap = [	4, 4, \
		3, 3, \
		5, 5, \
		2, 2, \
		6, 6, \
		1, 1, \
		7, 7, \
		0, 0 \
	]
	

def main(argv):
	inputFile = open(argv[1], "r")
	outputFile = open(argv[2], "w")

	regex = re.compile(r'([0-9]+)\s([0-9]+)\s([0-9]+)\s([0-9]+)\s([0-9\.]+)\s([0-9\.]+)\s([0-9\.]+)\s([0-9]+).*')
	lines = [ line for line in inputFile]
	for board in range(2):
		for line in lines:			
			m = regex.match(line)
			assert m is not None
			
			channel, region, xi, yi, x, y, z, hv = m.groups()
			channel = int(channel)
			region = int(region)
			xi = int(xi)
			yi = int(yi)
			x = float(x)
			y = float(x)
			z = float(z)
			hv = int(hv)

			channel = 128 * board + channel
			region = board

			# Mirror board
			if board == 1:
				xi = 15 - xi
				x = (3.6 * 15) - x

			if board == 0:
				z = -50.0
			else:
				z = 50.0
			hv = hvMap[hv] + 8 * board

			outputFile.write("%d\t%d\t%d\t%d\t%f\t%f\t%f\t%d\n" % (channel, region, xi, yi, x, y, z, hv))

	outputFile.close()
	inputFile.close()
		


if __name__ == '__main__':
	main(argv)