# -*- coding: utf-8 -*-
import ROOT
from array import array

class DataFile:
    def	__init__(self, rootFile, suffix):
      self.__rootFile = rootFile
      self.__dataTree = ROOT.TTree("data" + suffix, "Data " + suffix)
      self.__step1 = array('f', [0])
      self.__dataTree.Branch("step1", self.__step1, "step1/F")
      self.__step2 = array('f', [0])
      self.__dataTree.Branch("step2", self.__step2, "step2/F")
      self.__frame = array('L', [0])
      self.__dataTree.Branch("frame", self.__frame, "frame/L")

      self.__asic = array('i', [0])
      self.__dataTree.Branch("asic", self.__asic, "asic/I")
      self.__channel = array('i', [0])
      self.__dataTree.Branch("channel", self.__channel, "channel/I")
      self.__tac = array('i', [0])
      self.__dataTree.Branch("tac", self.__tac, "tac/I")
      self.__tcoarse = array('i', [0])
      self.__dataTree.Branch("tcoarse", self.__tcoarse, "tcoarse/I")
      self.__ecoarse = array('i', [0])
      self.__dataTree.Branch("ecoarse", self.__ecoarse, "ecoarse/I")
      self.__tfine = array('i', [0])
      self.__dataTree.Branch("tfine", self.__tfine, "tfine/I")
      self.__efine = array('i', [0])
      self.__dataTree.Branch("efine", self.__efine, "efine/I")
      self.__tacIdleTime = array('l', [0])
      self.__dataTree.Branch("tacIdleTime", self.__tacIdleTime, "tacIdleTime/L")
      self.__channelIdleTime = array('l', [0])
      self.__dataTree.Branch("channelIdleTime", self.__channelIdleTime, "channelIdleTime/L")


      self.__indexTree = ROOT.TTree("index3" + suffix, "Index v3 " + suffix)
      self.__indexTree.Branch("step1", self.__step1, "step1/F")
      self.__indexTree.Branch("step2", self.__step2, "step2/F")
      self.__stepBegin = array('i', [0])
      self.__indexTree.Branch("stepBegin", self.__stepBegin, "stepBegin/I")
      self.__stepEnd = array('i', [0])
      self.__indexTree.Branch("stepEnd", self.__stepEnd, "stepEnd/I")
      
      self.__firstEvent = True

      return None

    def addEvent(self, step1, step2, frame, asic, channel, tac, tcoarse, ecoarse, tfine, efine, channelIdleTime, tacIdleTime):
      if self.__firstEvent:
	self.__firstEvent = False
	self.__step1[0] = step1
	self.__step2[0] = step2
	self.__stepBegin[0] = 0
	self.__stepEnd[0] = 0

      a1 = array('f', [step1])
      a2 =  array('f', [step2]) 
      if self.__step1 != a1 or self.__step2 != a2:
	#print "Adding old step ", self.__step1[0],  self.__step2[0], self.__stepBegin[0], self.__stepEnd[0]
	self.__indexTree.Fill()
	self.__stepBegin[0] = self.__stepEnd[0]

      self.__step1[0] = step1
      self.__step2[0] = step2
      self.__frame[0] = frame
      self.__asic[0] = asic
      self.__channel[0] = channel
      self.__tac[0] = tac
      self.__tcoarse[0] = tcoarse
      self.__ecoarse[0] = ecoarse
      self.__tfine[0] = tfine
      self.__efine[0] = efine
      self.__channelIdleTime[0] = channelIdleTime
      self.__tacIdleTime[0] = tacIdleTime

      self.__dataTree.Fill()
      self.__stepEnd[0] = self.__stepEnd[0] + 1
      return None

    def write(self):
      self.__rootFile.Write()
      return None

    def close(self):
      self.__indexTree.Fill()      
      return None

      
  

if __name__ == '__main__':
  d = DataFile("/tmp/z2.root");
  d.addEvent(0, 0, 2, 3, 3, 3, 4, 4, 4, 5, 5)
  d.write()
  d.close()