# -*- coding: utf-8 -*-
import argparse
import atb
from loadLocalConfig import loadLocalConfig

parser = argparse.ArgumentParser(description='Set all active HV DACs to the same voltage')
parser.add_argument('ReqVoltage', type=float,
                   help='The requested voltage in Volts ')

args = parser.parse_args()

uut = atb.ATB("/tmp/d.sock")
uut.config = loadLocalConfig(useBaseline=False)
uut.setAllHVDAC(args.ReqVoltage)



