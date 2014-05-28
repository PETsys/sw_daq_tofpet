import re

def readBaselineProposal(fileName):
	print "Reading %s for ASIC %d" % (fileName, asic)
	f = open(fileName, "r")
	r = re.compile('[ \t\n\r:]*')
	for i in range(64):
		l = f.readline()
		s =  r.split(l)
		s = s[0:4]
		print s
		a, c, baseline, noise = s
		v = float(baseline)
		sigma= float(noise)
	f.close()


readBaselineProposal("M0.baseline")
readBaselineProposal("M1.baseline")
