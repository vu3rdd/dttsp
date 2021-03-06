#/usr/bin/env python

import sys
from sdr1khw import *

FALSE = 0
TRUE = 1

openPort("/dev/parport0")
DttSP_SampleRate = 48000.0

setRFE_Enabled(TRUE)
ResetRFE()
setXVTR_Enabled(FALSE)
setExtended(FALSE)
setXVTR_TR_Relay(FALSE)
setBandRelay(bs0)

Init()

setVFOOffset(0.0)
setATTN_Relay(TRUE)
setTransmitRelay(FALSE)
setGainRelay(TRUE)
setSpurReduction(FALSE)
setExternalOutput(FALSE)
setDDSClockCorrection(0.0)
setDDSFreq(13.845)
setMuteRelay(FALSE)

print "Ready"

cmd = sys.stdin.readline()
while not cmd == '':
    res = eval(cmd)
    print res
    cmd = sys.stdin.readline()

closePort()
