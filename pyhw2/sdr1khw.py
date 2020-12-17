# This file was created automatically by SWIG.
# Don't modify this file, modify the SWIG interface instead.
# This file is compatible with both classic and new-style classes.

import _sdr1khw

def _swig_setattr(self,class_type,name,value):
    if (name == "this"):
        if isinstance(value, class_type):
            self.__dict__[name] = value.this
            if hasattr(value,"thisown"): self.__dict__["thisown"] = value.thisown
            del value.thisown
            return
    method = class_type.__swig_setmethods__.get(name,None)
    if method: return method(self,value)
    self.__dict__[name] = value

def _swig_getattr(self,class_type,name):
    method = class_type.__swig_getmethods__.get(name,None)
    if method: return method(self)
    raise AttributeError,name

import types
try:
    _object = types.ObjectType
    _newclass = 1
except AttributeError:
    class _object : pass
    _newclass = 0
del types


FALSE = _sdr1khw.FALSE
TRUE = _sdr1khw.TRUE
IARU1 = _sdr1khw.IARU1
IARU2 = _sdr1khw.IARU2
IARU3 = _sdr1khw.IARU3
BYPASS = _sdr1khw.BYPASS
MEMORY = _sdr1khw.MEMORY
FULL = _sdr1khw.FULL
LAST = _sdr1khw.LAST
POSITIVE = _sdr1khw.POSITIVE
NEGATIVE = _sdr1khw.NEGATIVE
NONE = _sdr1khw.NONE
bsnone = _sdr1khw.bsnone
bs0 = _sdr1khw.bs0
bs1 = _sdr1khw.bs1
bs2 = _sdr1khw.bs2
bs3 = _sdr1khw.bs3
bs4 = _sdr1khw.bs4
bs5 = _sdr1khw.bs5
EXT = _sdr1khw.EXT
BPF = _sdr1khw.BPF
DAT = _sdr1khw.DAT
ADR = _sdr1khw.ADR
PIN_12 = _sdr1khw.PIN_12
Dash = _sdr1khw.Dash
Dot = _sdr1khw.Dot
PA_DATA = _sdr1khw.PA_DATA
PIN_11 = _sdr1khw.PIN_11
P1 = _sdr1khw.P1
P2 = _sdr1khw.P2
P3 = _sdr1khw.P3
P4 = _sdr1khw.P4
P5 = _sdr1khw.P5
P6 = _sdr1khw.P6
P7 = _sdr1khw.P7
SER = _sdr1khw.SER
SCK = _sdr1khw.SCK
SCLR_NOT = _sdr1khw.SCLR_NOT
DCDR_NE = _sdr1khw.DCDR_NE
IC11 = _sdr1khw.IC11
IC7 = _sdr1khw.IC7
IC9 = _sdr1khw.IC9
IC10 = _sdr1khw.IC10
LPF0 = _sdr1khw.LPF0
LPF1 = _sdr1khw.LPF1
LPF2 = _sdr1khw.LPF2
LPF3 = _sdr1khw.LPF3
LPF4 = _sdr1khw.LPF4
LPF5 = _sdr1khw.LPF5
LPF6 = _sdr1khw.LPF6
LPF7 = _sdr1khw.LPF7
LPF8 = _sdr1khw.LPF8
LPF9 = _sdr1khw.LPF9
BPF0 = _sdr1khw.BPF0
BPF1 = _sdr1khw.BPF1
BPF2 = _sdr1khw.BPF2
BPF3 = _sdr1khw.BPF3
BPF4 = _sdr1khw.BPF4
BPF5 = _sdr1khw.BPF5
PAF0 = _sdr1khw.PAF0
PAF1 = _sdr1khw.PAF1
PAF2 = _sdr1khw.PAF2
ADC_CLK = _sdr1khw.ADC_CLK
ADC_DI = _sdr1khw.ADC_DI
ADC_CS_NOT = _sdr1khw.ADC_CS_NOT
PATR = _sdr1khw.PATR
ATUCTL = _sdr1khw.ATUCTL
AMP_RLYS = _sdr1khw.AMP_RLYS
XVTR_TR_RLY = _sdr1khw.XVTR_TR_RLY
XVTR_RLY = _sdr1khw.XVTR_RLY
ATTN_RLY = _sdr1khw.ATTN_RLY
IMPULSE_RLY = _sdr1khw.IMPULSE_RLY
PA_BIAS_NOT = _sdr1khw.PA_BIAS_NOT
IMPULSE = _sdr1khw.IMPULSE
PA_LPF_OFF = _sdr1khw.PA_LPF_OFF
PA_LPF_12_10 = _sdr1khw.PA_LPF_12_10
PA_LPF_17_15 = _sdr1khw.PA_LPF_17_15
PA_LPF_30_20 = _sdr1khw.PA_LPF_30_20
PA_LPF_60_40 = _sdr1khw.PA_LPF_60_40
PA_LPF_80 = _sdr1khw.PA_LPF_80
PA_LPF_160 = _sdr1khw.PA_LPF_160
PA_FORWARD_PWR = _sdr1khw.PA_FORWARD_PWR
PA_REVERSE_PWR = _sdr1khw.PA_REVERSE_PWR
TR = _sdr1khw.TR
MUTE = _sdr1khw.MUTE
GAIN = _sdr1khw.GAIN
WRB = _sdr1khw.WRB
RESET = _sdr1khw.RESET
COMP_PD = _sdr1khw.COMP_PD
DIG_PD = _sdr1khw.DIG_PD
BYPASS_PLL = _sdr1khw.BYPASS_PLL
INT_IOUD = _sdr1khw.INT_IOUD
OSK_EN = _sdr1khw.OSK_EN
OSK_INT = _sdr1khw.OSK_INT
BYPASS_SINC = _sdr1khw.BYPASS_SINC
PLL_RANGE = _sdr1khw.PLL_RANGE
SDR1K_LATCH_EXT = _sdr1khw.SDR1K_LATCH_EXT
SDR1K_LATCH_BPF = _sdr1khw.SDR1K_LATCH_BPF

openPort = _sdr1khw.openPort

closePort = _sdr1khw.closePort

USB_Sdr1kLatch = _sdr1khw.USB_Sdr1kLatch

USB_Sdr1kGetStatusPort = _sdr1khw.USB_Sdr1kGetStatusPort

USB_Sdr1kGetADC = _sdr1khw.USB_Sdr1kGetADC

USB_Sdr1kDDSReset = _sdr1khw.USB_Sdr1kDDSReset

USB_Sdr1kDDSWrite = _sdr1khw.USB_Sdr1kDDSWrite

USB_Sdr1kSRLoad = _sdr1khw.USB_Sdr1kSRLoad

DttSP_ChangeOsc = _sdr1khw.DttSP_ChangeOsc

Init = _sdr1khw.Init

PowerOn = _sdr1khw.PowerOn

StandBy = _sdr1khw.StandBy

Impulse = _sdr1khw.Impulse

StatusPort = _sdr1khw.StatusPort

SetExt = _sdr1khw.SetExt

ResExt = _sdr1khw.ResExt

PinValue = _sdr1khw.PinValue

SetBPF = _sdr1khw.SetBPF

TestPort = _sdr1khw.TestPort

RCKStrobe = _sdr1khw.RCKStrobe

SRLoad = _sdr1khw.SRLoad

ResetRFE = _sdr1khw.ResetRFE

PA_SetLPF = _sdr1khw.PA_SetLPF

PA_GetADC = _sdr1khw.PA_GetADC

PA_ATUTune = _sdr1khw.PA_ATUTune

getEnableLPF0 = _sdr1khw.getEnableLPF0

setEnableLPF0 = _sdr1khw.setEnableLPF0

getExtended = _sdr1khw.getExtended

setExtended = _sdr1khw.setExtended

getX2Enabled = _sdr1khw.getX2Enabled

setX2Enabled = _sdr1khw.setX2Enabled

getX2Delay = _sdr1khw.getX2Delay

setX2Delay = _sdr1khw.setX2Delay

getRFE_Enabled = _sdr1khw.getRFE_Enabled

setRFE_Enabled = _sdr1khw.setRFE_Enabled

getPA_Enabled = _sdr1khw.getPA_Enabled

setPA_Enabled = _sdr1khw.setPA_Enabled

getXVTR_Enabled = _sdr1khw.getXVTR_Enabled

setXVTR_Enabled = _sdr1khw.setXVTR_Enabled

getUSB_Enabled = _sdr1khw.getUSB_Enabled

setUSB_Enabled = _sdr1khw.setUSB_Enabled

getCurrentXVTRTRMode = _sdr1khw.getCurrentXVTRTRMode

setCurrentXVTRTRMode = _sdr1khw.setCurrentXVTRTRMode

getLatchDelay = _sdr1khw.getLatchDelay

setLatchDelay = _sdr1khw.setLatchDelay

getMinFreq = _sdr1khw.getMinFreq

getMaxFreq = _sdr1khw.getMaxFreq

getBaseAddr = _sdr1khw.getBaseAddr

setBaseAddr = _sdr1khw.setBaseAddr

getBandRelay = _sdr1khw.getBandRelay

setBandRelay = _sdr1khw.setBandRelay

getTransmitRelay = _sdr1khw.getTransmitRelay

setTransmitRelay = _sdr1khw.setTransmitRelay

getMuteRelay = _sdr1khw.getMuteRelay

setMuteRelay = _sdr1khw.setMuteRelay

getGainRelay = _sdr1khw.getGainRelay

setGainRelay = _sdr1khw.setGainRelay

getExternalOutput = _sdr1khw.getExternalOutput

setExternalOutput = _sdr1khw.setExternalOutput

getDDSClockCorrection = _sdr1khw.getDDSClockCorrection

setDDSClockCorrection = _sdr1khw.setDDSClockCorrection

getPLLMult = _sdr1khw.getPLLMult

setPLLMult = _sdr1khw.setPLLMult

getDDSClock = _sdr1khw.getDDSClock

setDDSClock = _sdr1khw.setDDSClock

getIFShift = _sdr1khw.getIFShift

setIFShift = _sdr1khw.setIFShift

getSpurReduction = _sdr1khw.getSpurReduction

setSpurReduction = _sdr1khw.setSpurReduction

getIFFreq = _sdr1khw.getIFFreq

setIFFreq = _sdr1khw.setIFFreq

getDDSFreq = _sdr1khw.getDDSFreq

setDDSFreq = _sdr1khw.setDDSFreq

getSampleRate = _sdr1khw.getSampleRate

setSampleRate = _sdr1khw.setSampleRate

getFFTLength = _sdr1khw.getFFTLength

setFFTLength = _sdr1khw.setFFTLength

getTuneFFT = _sdr1khw.getTuneFFT

getTuneFracRel = _sdr1khw.getTuneFracRel

getVFOOffset = _sdr1khw.getVFOOffset

setVFOOffset = _sdr1khw.setVFOOffset

getIOUDClock = _sdr1khw.getIOUDClock

setIOUDClock = _sdr1khw.setIOUDClock

getDACMult = _sdr1khw.getDACMult

setDACMult = _sdr1khw.setDACMult

getAMP_Relay = _sdr1khw.getAMP_Relay

setAMP_Relay = _sdr1khw.setAMP_Relay

getATTN_Relay = _sdr1khw.getATTN_Relay

setATTN_Relay = _sdr1khw.setATTN_Relay

getXVTR_TR_Relay = _sdr1khw.getXVTR_TR_Relay

setXVTR_TR_Relay = _sdr1khw.setXVTR_TR_Relay

getXVTR_Relay = _sdr1khw.getXVTR_Relay

setXVTR_Relay = _sdr1khw.setXVTR_Relay

getIMPULSE_Relay = _sdr1khw.getIMPULSE_Relay

setIMPULSE_Relay = _sdr1khw.setIMPULSE_Relay

getPA_TransmitRelay = _sdr1khw.getPA_TransmitRelay

setPA_TransmitRelay = _sdr1khw.setPA_TransmitRelay

getPA_BiasOn = _sdr1khw.getPA_BiasOn

setPA_BiasOn = _sdr1khw.setPA_BiasOn
cvar = _sdr1khw.cvar

