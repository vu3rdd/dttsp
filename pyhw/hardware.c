/* hardware.c */

#include <hardware.h>

Rig myRig;

PRIVATE void CalcClock(void);
PRIVATE void Latch(CtrlPin vCtrlPin);
PRIVATE void PWrite(BYTE data);
PRIVATE void DDSWrite(BYTE data, BYTE addr);
PRIVATE void SetRadioFreq(double f);
PRIVATE void Delay(void);
PRIVATE void ResetLatches(void);
PRIVATE void ResetDDS(void);
BOOLEAN InputPin(StatusPin vStatusPin);

static int counter = 0;

// ======================================================
// Properties
// ======================================================

BOOLEAN
getExtended(void) {
  return myRig.extended;
}

void
setExtended(BOOLEAN value) {
  myRig.extended = value;
}

BOOLEAN
getRFE_Enabled(void) {
  return myRig.rfe_enabled;
}

void
setRFE_Enabled(BOOLEAN value) {
  myRig.rfe_enabled = value;
}

BOOLEAN
getXVTR_Enabled(void) {
  return myRig.xvtr_enabled;
}

void
setXVTR_Enabled(BOOLEAN value) {
  myRig.xvtr_enabled = value;
  if (myRig.xvtr_enabled)
    myRig.max_freq = 146.0;
  else
    myRig.max_freq = 65.0;
}

//    private BOOLEANxvtr_tr_logic = FALSE;
BOOLEAN
getXVTR_TR_Logic(void) {
  return myRig.xvtr_tr_logic;
}

void
setXVTR_TR_Logic(BOOLEAN value) {
  myRig.xvtr_tr_logic = value;
}

int
getLatchDelay(void) {
  return myRig.latch_delay;
}

void
setLatchDelay(int value) {
  myRig.latch_delay = value;
}

double
getMinFreq(void) {
  return myRig.min_freq;
}

double
getMaxFreq(void) {
  return myRig.max_freq;
}

/* unsigned short
getBaseAddr(void) {
  return myRig.baseAdr;
}

void
setBaseAddr(unsigned short value) {
  myRig.baseAdr = value;
  }*/

BandSetting
getBandRelay(void) {
  return myRig.band_relay;
}

void
setBandRelay(BandSetting value) {

  extern void Pwrite(unsigned char);
  myRig.band_relay = value;
  PWrite((unsigned char) (myRig.band_relay + myRig.transmit_relay +
			  myRig.mute_relay));
  Latch(BPF);
}

BOOLEAN
getTransmitRelay(void) {
  //Get state of TR relay on BPF board
  if (myRig.transmit_relay == TR)
    return TRUE;
  else
    return FALSE;
}

void
setTransmitRelay(BOOLEAN value) {
  //If in valid Amateur BandRelay Save and output new TR Relay setting
  BYTE tmpLatch = 0;
  BOOLEAN tmpATTN = FALSE;
  if (value == TRUE) {
    if (IsHamBand(myRig.curBandPlan) == TRUE) {
      myRig.transmit_relay = TR;	// Set to TX
      if (getRFE_Enabled()) {
	tmpATTN = getATTN_Relay();
	if (getXVTR_Enabled() && (myRig.dds_freq >= 144))
	  setXVTR_TR_Relay(myRig.xvtr_tr_logic);	//Set XVTR TR to transmit
	else
	  setAMP_Relay(TRUE);	//Switch RFE to transmit

	// TODO: REMOVE THE AMP RELAY SWITCH ON THE PRODUCTION RFE

	tmpLatch = (BYTE) (myRig.transmit_relay + myRig.mute_relay);
      } else
	tmpLatch =
	  (BYTE) (myRig.band_relay + myRig.transmit_relay +
		  myRig.mute_relay);
    }
  } else {
    myRig.transmit_relay = 0;	// Set to RX
    if (getRFE_Enabled()) {
      setAMP_Relay(FALSE);
      tmpLatch = (BYTE) (myRig.transmit_relay + myRig.mute_relay);
      if (getXVTR_Enabled())
	setXVTR_TR_Relay(!myRig.xvtr_tr_logic);	// Set XVTR TR to receive
      setATTN_Relay(tmpATTN);
    } else
      tmpLatch =
	(BYTE) (myRig.band_relay + myRig.transmit_relay +
		myRig.mute_relay);
  }
  PWrite(tmpLatch);
  Latch(BPF);
}

BOOLEAN
getMuteRelay(void) {
//Get state of MUTE relay on TRX board
  if (myRig.mute_relay == MUTE)
    return FALSE;
  else
    return TRUE;
}

void
setMuteRelay(BOOLEAN value) {
  //Mute the speaker relay if TRUE
  if (value == TRUE)
    myRig.mute_relay = 0;
  else
    myRig.mute_relay = MUTE;
  PWrite((BYTE)
	 (myRig.band_relay + myRig.transmit_relay + myRig.mute_relay));
  Latch(BPF);
}

BOOLEAN
getGainRelay(void) {
  //Get state of GAIN relay on TRX board
  if (myRig.gain_relay == GAIN)
    return FALSE;
  else
    return TRUE;
}

void
setGainRelay(BOOLEAN value) {				//Save and output state of GAIN relay on TRX board
  if (value == TRUE)
    myRig.gain_relay = 0;	// 40dB or 0dB w/RFE
  else
    myRig.gain_relay = GAIN;	// 26dB
  PWrite((BYTE) (myRig.external_output + myRig.gain_relay));
  Latch(EXT);
}

int
getExternalOutput(void) {				//Get state of External Control outputs on PIO board
  return myRig.external_output;
}

void
setExternalOutput(int value) {
  //Save and output state of External Control outputs on PIO board
  myRig.external_output = value;
  PWrite((BYTE) (myRig.external_output + myRig.gain_relay));
  Latch(EXT);
}

double
getDDSClockCorrection(void) {
  return myRig.dds_clock_correction;
}

void
setDDSClockCorrection(double value) {
  myRig.dds_clock_correction = value;
  CalcClock();
  SetRadioFreq(myRig.dds_freq);
}

int
getPLLMult(void) {
  return myRig.pll_mult;
}

PRIVATE void
CalcClock(void) {
  //sysClock = (pll_mult * dds_clock);
  myRig.sysClock = (myRig.pll_mult * myRig.dds_clock)
    + myRig.dds_clock_correction;
  //Calculates step size for 16 bit frequency step size
  myRig.dds_step_size = myRig.sysClock / pow(2.0, 48.0);
}

void
setPLLMult(int value) {
  myRig.pll_mult = value;
  CalcClock();
}

double
getDDSClock(void) {
  return myRig.dds_clock;
}

void
setDDSClock(double value) {
  //Compute internal DDS System Clock and Phase Truncation Elimination Step
  myRig.dds_clock = value;
  CalcClock();
}

BOOLEAN
getIFShift(void) {
  return myRig.if_shift;
}

void
setIFShift(BOOLEAN value) {
  //Turns IF shift on and off
  myRig.if_shift = value;
  if (!myRig.spur_reduction) {
    if (myRig.if_shift) {
      myRig.OSC_change = -11025.0;
      myRig.needs_OSC_change = TRUE;
    } else {
      myRig.OSC_change = 0.0;
      myRig.needs_OSC_change = TRUE;
    }
  } else
  myRig.needs_OSC_change = FALSE;
  SetRadioFreq(myRig.dds_freq);
}

BOOLEAN
getSpurReduction(void) {
  return myRig.spur_reduction;
}

void
setSpurReduction(BOOLEAN value) {
  //Turns DDS Phase Truncation Spur reduction on and off
  myRig.spur_reduction = value;
  if (!myRig.spur_reduction) {
    if (myRig.if_shift) {
      myRig.OSC_change = -11025.0;
      myRig.needs_OSC_change = TRUE;
    } else {
      myRig.OSC_change = 0.0;
      myRig.needs_OSC_change = TRUE;
    }
  } else
  myRig.needs_OSC_change = FALSE;
  SetRadioFreq(myRig.dds_freq);
}

double
getIFFreq(void) {
  return myRig.if_freq;
}

void
setIFFreq(double value) {
  //Sets the offset frequency for the IF in MHz
  myRig.if_freq = value;
}

double
getDDSFreq(void) {
  return myRig.dds_freq;
}

void
setDDSFreq(double value) {
  myRig.dds_freq = value;
  SetRadioFreq(myRig.dds_freq);
}

int
getSampleRate(void) {
  return myRig.sample_rate;
}

void
setSampleRate(int value) {
  myRig.sample_rate = value;
  //Compute bandwidth of FFT bin
  if (myRig.fft_length > 0)
    myRig.FFT_Bin_Size = (myRig.sample_rate / myRig.fft_length) * 1e-6;
}

int
getFFTLength(void) {
  return myRig.fft_length;
}

void
setFFTLength(int value) {
  myRig.fft_length = value;
  //Compute bandwidth of FFT bin
  if (myRig.fft_length > 0)
    myRig.FFT_Bin_Size = (myRig.sample_rate / myRig.fft_length) * 1e-6;
}

int
getTuneFFT(void) {
  return myRig.tune_fft;
}

double
getTuneFracRel(void) {
  return myRig.tune_frac_rel;
}

double
getVFOOffset(void) {
  return myRig.vfo_offset;
}

void
setVFOOffset(double value) {
  myRig.vfo_offset = value;
  SetRadioFreq(myRig.dds_freq);
}

int
getIOUDClock(void) {
  return myRig.ioud_clock;
}

void
setIOUDClock(int value) {
  double bitVal, bytVal;
  int i;
  BYTE lWord;
  myRig.ioud_clock = value;	//Save value

  bitVal = value;		//Compute Numeric Value

  for (i = 24; i >= 0; i -= 8)	//Convert to binary strings
  {
    // bytVal = bitVal / (Math.Pow(2, i));  //Compute binary byte Value
    bytVal = bitVal / pow(2.0, (double) i);	//Compute binary byte Value
    lWord = (BYTE) bytVal;	//Truncate fractional portion
    bitVal -= lWord * pow(2.0, (double) i);	//Reduce value
    switch (i)			//Write to byte position
    {
    case 32:
      DDSWrite(lWord, 22);
      break;
    case 16:
      DDSWrite(lWord, 23);
      break;
    case 8:
      DDSWrite(lWord, 24);
      break;
    case 0:
      DDSWrite(lWord, 25);
      break;
    }
  }
}

unsigned short
getDACMult(void) {
  return myRig.dac_mult;
}

void
setDACMult(unsigned short value) {
  double bitVal, bytVal;
  int i;
  BYTE lWord;
  //Send new I DAC Multiplier value to DDS
  myRig.dac_mult = value;
  bitVal = value;		//Compute Numeric Value
  for (i = 8; i >= 0; i -= 8)	//Convert to binary strings
  {
    bytVal = bitVal / pow(2.0, (double) i);	//Compute binary byte Value
    lWord = (BYTE) bytVal;	//Truncate fractional portion
    bitVal -= lWord * pow(2.0, (double) i);	//Reduce value
    switch (i) {
    case 8:
      DDSWrite(lWord, 33);
      break;
    case 0:
      DDSWrite(lWord, 34);
      break;
    }
  }

  bitVal = value;		//Compute Numeric Value
  for (i = 8; i >= 0; i -= 8)	//Convert to binary strings
  {
    bytVal = bitVal / pow(2.0, (double) i);	//Compute binary byte Value
    lWord = (BYTE) bytVal;	//Truncate fractional portion
    bitVal -= lWord * pow(2.0, (double) i);	//Reduce value
    switch (i) {
    case 8:
      DDSWrite(lWord, 35);
      break;
    case 0:
      DDSWrite(lWord, 36);
      break;
    }
  }
}

// ======================================================
// Private Member Functions
// ======================================================

PRIVATE void
Delay(void) {
  usleep(1000 * myRig.latch_delay);
}

PRIVATE void
Latch(CtrlPin vCtrlPin) {
  unsigned char mask;
  //Strobe the specified pin to latch data
  switch (vCtrlPin) {
  case EXT:
    mask = 0xA, ioctl(myRig.fd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(myRig.fd, PPWCONTROL, &mask);
    break;
  case BPF:
    mask = 0x9, ioctl(myRig.fd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(myRig.fd, PPWCONTROL, &mask);
    break;
  case DAT:
    mask = 0xF, ioctl(myRig.fd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(myRig.fd, PPWCONTROL, &mask);
    break;
  case ADR:
    mask = 0x3, ioctl(myRig.fd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(myRig.fd, PPWCONTROL, &mask);
    break;
  }
}

PRIVATE void
ResetLatches(void) {				//Set all latch outputs to logic zero (relays off)
  PWrite(0);
  Latch(ADR);
  Latch(DAT);

  myRig.gain_relay = GAIN;
  myRig.external_output = 0;
  PWrite((BYTE) (myRig.external_output + myRig.gain_relay));
  Latch(EXT);

  setBandRelay(bsnone);
  myRig.transmit_relay = 0;
  myRig.mute_relay = MUTE;

  PWrite((BYTE)
	 (myRig.band_relay + myRig.transmit_relay + myRig.mute_relay));
  Latch(BPF);
}

PRIVATE void
PWrite(BYTE data) {				//Write data Byte to parallel port
  ioctl(myRig.fd, PPWDATA, &data);
  Delay();			//Delay to allow data line setup
}

PRIVATE void
ResetDDS(void) {
  PWrite(RESET + WRB);		//Reset the DDS chip
  Latch(ADR);
  PWrite(WRB);			//Leave WRB high
  Latch(ADR);
  DDSWrite(COMP_PD, 29);	//Power down comparator
  //TODO: ADD PLL MULTIPLIER PROPERTY AND LOGIC
  DDSWrite(BYPASS_PLL, 30);	//Bypass PLL
  //DDSWrite(BYPASS_SINC + OSK_EN, 32);//Bypass Inverse Sinc and enable DAC Mult
  DDSWrite(BYPASS_SINC, 32);
  setDACMult(4095);		//Set DAC multiplier value
}

PRIVATE void
DDSWrite(BYTE data, BYTE addr) {
  //Set up data bits
  PWrite(data);
  Latch(DAT);

  //Set up address bits with WRB high
  PWrite((BYTE) (addr + WRB));
  Latch(ADR);

  //Send write command with WRB low
  PWrite(addr);
  Latch(ADR);

  //Return WRB high
  PWrite(WRB);
  Latch(ADR);
}

PRIVATE void
SetRadioFreq(double f) {
  double vfoFreq = f;
  int i;
  // calculate software frequency to program
  if (getXVTR_Enabled() && f >= 144 && f <= 146)	//If transverter enabled compute 28MHz IF frequency
    f -= 116;			//Subtract 116MHz (144-28) from VFO display frequency

  if (myRig.if_shift)
    f -= myRig.if_freq;	// adjust for IF shift

  f += myRig.vfo_offset;	// adjust for vfo offset

  unsigned long long int tuning_word =
    (unsigned long long int) (f / myRig.sysClock * pow(2.0, 48.0));

  // start with current tuning word
  // clear first bit, low 31 bits; set bit 31
  if (myRig.spur_reduction) {
    unsigned long long int sr_tuning_word =
      (tuning_word & ~(0x80007fffffffLL)) | 0x000080000000LL;
    double software_offset =
      (sr_tuning_word - tuning_word) * myRig.dds_step_size;

    if (myRig.if_shift)	//Convert the tuning fraction to rel frq
      myRig.tune_frac_rel = 1000000.0 * (software_offset) - 11025.0;
    else
      myRig.tune_frac_rel = 1000000.0 * (software_offset);
    myRig.OSC_change = myRig.tune_frac_rel;
    myRig.needs_OSC_change = TRUE;

    tuning_word = sr_tuning_word;
  }
  // program hardware
  SetBPF(vfoFreq);

  if (tuning_word != myRig.last_tuning_word) {
    //Debug.WriteLine("tuning_word != last_tuning_word");
    //Debug.Write("tuning word: ");

    myRig.last_tuning_word = tuning_word;	//save new tuning word    

    for (i = 0; i < 6; i++) {
      BYTE b =
	(BYTE) (tuning_word >> (unsigned long long int) (40 - i * 8)) &
	0xFFLL;
      //Debug.Write(b+" ");
      DDSWrite(b, (BYTE) (4 + i));
    }
    //Debug.WriteLine("");
  }
}

// ======================================================
// Public Member Functions
// ======================================================

BOOLEAN
InputPin(StatusPin vStatusPin) {				//Readback state and mask specified status pin
  unsigned char status;
  ioctl(myRig.fd, PPRSTATUS, &status);
  if (vStatusPin == PIN_11)	//Pin 11 is inverted
    return (((BYTE) vStatusPin & status) == 0);
  else
    return (((BYTE) vStatusPin & status) != 0);
}

BYTE
StatusPort(void) {
  unsigned char status;
  ioctl(myRig.fd, PPRSTATUS, &status);
  return status;
}

void
RigInit(void) {
  setIFShift(TRUE);

  // user setable through the setup form
  //DDSClockCorrection = 0.000;
  setPLLMult(1);
  setDDSClock(200.0);
  setIFFreq(0.011025);
  setSampleRate(48000);
  setFFTLength(4096);
  setDACMult(4095);

  //                      ResetLatches(void);         //Reset all control latches
  //
  //                      if(RFE_Enabled)
  //                      {
  //                              ResetRFE(void);
  //                              if(XVTR_Enabled)
  //                                      XVTR_TR_Relay = TRUE;  //Set transverter to receive mode
  //                      }
  //                      ResetDDS(void);                     //Hardware reset for DDS                        
}

void
PowerOn(void) {
  // set mute/gain relays based on console
  if (getXVTR_Enabled())
    setXVTR_TR_Relay(!myRig.xvtr_tr_logic);
  setDDSFreq(myRig.dds_freq);
}

void
StandBy(void) {
  ResetLatches();
  if (getRFE_Enabled())
    ResetRFE();
  if (getXVTR_Enabled())
    setXVTR_TR_Relay(FALSE);
  ResetDDS();
}

void
SetExt(ExtPin pin) {
  //Set the designated external pin high
  myRig.external_output += (BYTE) pin;
  PWrite((BYTE) (myRig.external_output + myRig.gain_relay));
  Latch(EXT);
}

void
ResExt(ExtPin pin) {
  //Reset the designated external pin high
  myRig.external_output -= (BYTE) pin;
  PWrite((BYTE) (myRig.external_output + myRig.gain_relay));
  Latch(EXT);
}

BOOLEAN
PinValue(ExtPin pin) {
  //Return TRUE if Pin is set
  if ((myRig.external_output & (int) pin) != 0)
    return TRUE;
  else
    return FALSE;
}

void
SetBPF(double VFOValue) {
  if (getRFE_Enabled())		// RFE is present
  {
    //Use shift registers on RFE to control BPF and LPF banks
    if (VFOValue <= 2)		// DC to 2MHz
    {
      SRLoad(IC10, LPF9 + BPF0);
      SRLoad(IC9, 0);
    } else if (VFOValue <= 4)	// 2MHz to 4MHz
    {
      SRLoad(IC10, BPF1);
      SRLoad(IC9, LPF7);
    } else if (VFOValue <= 6)	// 4MHz to 6MHz
    {
      SRLoad(IC10, BPF1);
      SRLoad(IC9, LPF2);
    } else if (VFOValue <= 7.3)	// 6MHz to 7.3MHz
    {
      SRLoad(IC10, BPF2);
      SRLoad(IC9, LPF5);
    } else if (VFOValue <= 10.2)	// 7.3MHz to 10.2MHz
    {
      SRLoad(IC10, BPF2);
      SRLoad(IC9, LPF4);
    } else if (VFOValue <= 12)	// 10.2MHz to 12MHz
    {
      SRLoad(IC10, BPF2);
      SRLoad(IC9, LPF3);
    } else if (VFOValue <= 14.5)	// 12MHz to 14.5MHz
    {
      SRLoad(IC10, BPF3);
      SRLoad(IC9, LPF3);
    } else if (VFOValue <= 21.5)	// 14.5MHz to 21.5MHz
    {
      SRLoad(IC10, BPF3 + LPF8);
      SRLoad(IC9, 0);
    } else if (VFOValue <= 24)	// 21.5MHz to 24MHz
    {
      SRLoad(IC10, BPF3);
      SRLoad(IC9, LPF6);
    } else if (VFOValue <= 30)	// 24MHz to 30MHz
    {
      SRLoad(IC10, BPF4);
      SRLoad(IC9, LPF6);
    } else if (VFOValue <= 36)	// 30MHz to 36MHz
    {
      SRLoad(IC10, BPF4);
      SRLoad(IC9, LPF1);
    } else if (VFOValue <= 65)	// 36MHz to 65Mhz
    {
      SRLoad(IC10, BPF5);
      SRLoad(IC9, LPF1);
    } else if (getXVTR_Enabled() && VFOValue >= 144 && VFOValue <= 146)	//28MHz IF for transverter
    {
      SRLoad(IC10, BPF4);
      SRLoad(IC9, LPF6);
      setXVTR_Relay(TRUE);
    }
    if (getXVTR_Enabled() && VFOValue < 144)
      setXVTR_Relay(FALSE);
  } else			// RFE is not present
  {
    //Select the BPF relays using the high frequency cutoff
    if (VFOValue < 2)		//DC to 2MHz
      setBandRelay(bs0);
    else if (VFOValue < 6)	//2MHz to 6MHz
      setBandRelay(bs1);
    else if (VFOValue < 12)	//6MHz to 12MHz
      setBandRelay(bs2);
    else if (VFOValue < 24)	//12MHz to 24MHz
      setBandRelay(bs3);
    else if (VFOValue < 36)	//24MHz to 36MHz
      setBandRelay(bs4);
    else			//36MHz to 65Mhz
      setBandRelay(bs5);
  }
}

BOOLEAN
IsHamBand(BandPlan b) {
  if (myRig.extended)
    return TRUE;

  switch (b) {
  case IARU1:
    if (myRig.dds_freq >= 1.8 && myRig.dds_freq <= 2.0)
      return TRUE;
    else if (myRig.dds_freq >= 3.5 && myRig.dds_freq <= 4.0)
      return TRUE;
    else if (myRig.dds_freq == 5.3305)
      return TRUE;
    else if (myRig.dds_freq == 5.3465)
      return TRUE;
    else if (myRig.dds_freq == 5.3665)
      return TRUE;
    else if (myRig.dds_freq == 5.3715)
      return TRUE;
    else if (myRig.dds_freq == 5.4035)
      return TRUE;
    else if (myRig.dds_freq >= 7.0 && myRig.dds_freq <= 7.3)
      return TRUE;
    else if (myRig.dds_freq >= 10.1 && myRig.dds_freq <= 10.15)
      return TRUE;
    else if (myRig.dds_freq >= 14.0 && myRig.dds_freq <= 14.35)
      return TRUE;
    else if (myRig.dds_freq >= 18.068 && myRig.dds_freq <= 18.168)
      return TRUE;
    else if (myRig.dds_freq >= 21.0 && myRig.dds_freq <= 21.45)
      return TRUE;
    else if (myRig.dds_freq >= 24.89 && myRig.dds_freq <= 24.99)
      return TRUE;
    else if (myRig.dds_freq >= 21.0 && myRig.dds_freq <= 21.45)
      return TRUE;
    else if (myRig.dds_freq >= 28.0 && myRig.dds_freq <= 29.7)
      return TRUE;
    else if (myRig.dds_freq >= 50.0 && myRig.dds_freq <= 54.0)
      return TRUE;
    else if (myRig.dds_freq >= 144.0 && myRig.dds_freq <= 146.0) {
      if (getRFE_Enabled() && getXVTR_Enabled())
	return TRUE;
      else
	return FALSE;
    } else
      return FALSE;
  default:
    return FALSE;
    // TODO: Implement other bandplans here
  }
}

void
TestPort(void) {
  //Toggle 1 and 0 to each of the four parallel port latches
  PWrite(0);
  Latch(BPF);
  Latch(ADR);
  Latch(DAT);
  Latch(EXT);
  PWrite(255);
  Latch(BPF);
  Latch(ADR);
  Latch(DAT);
  Latch(EXT);
}

void
RCKStrobe(BOOLEAN ClearReg, RFE_RCK Reg) {
  //Strobe the RFE 1:4 decoder output to transfer contents of shift register to output latches
  if (ClearReg)			//Clear the shift register contents
    PWrite((BYTE) (Reg));	//Strobe decoder output low and clear register
  else
    PWrite((BYTE) (SCLR_NOT + Reg + myRig.transmit_relay + myRig.mute_relay));	//Strobe decoder output low
  Latch(BPF);
  PWrite((BYTE) (SCLR_NOT + DCDR_NE + myRig.transmit_relay + myRig.mute_relay));	//Disable 1:4 decoder outputs
  Latch(BPF);
}

void
SRLoad(RFE_RCK Reg, int Data) {
  //Shift data into shift registers on RFE
  int choose[] = { 128, 64, 32, 16, 8, 4, 2, 1 };
  int i;

  for (i = 0; i < 8; i++) {
    int mask = choose[i];	//Mask the current bit
    if ((mask & Data) == 0)	//Current bit = 0
    {
      PWrite((BYTE) (SCLR_NOT + DCDR_NE + myRig.transmit_relay + myRig.mute_relay));	//Output 0 bit
      Latch(BPF);
      PWrite((BYTE) (SCLR_NOT + DCDR_NE + SCK + myRig.transmit_relay + myRig.mute_relay));	//Clock 0 into shift register
    } else			//Current bit = 1
    {
      PWrite((BYTE) (SCLR_NOT + DCDR_NE + SER + myRig.transmit_relay + myRig.mute_relay));	//Output 1 bit
      Latch(BPF);
      PWrite((BYTE) (SCLR_NOT + DCDR_NE + SER + SCK + myRig.transmit_relay + myRig.mute_relay));	//Clock 1 into shift register
    }
    Latch(BPF);
    PWrite((BYTE) (SCLR_NOT + DCDR_NE + myRig.transmit_relay + myRig.mute_relay));	//Return SCK low
    Latch(BPF);
  }
  RCKStrobe(FALSE, Reg);	//Strobe Register Clock
}

void
ResetRFE(void) {
  //Reset all RFE shift registers to zero output
  RCKStrobe(TRUE, IC11);
  RCKStrobe(TRUE, IC7);
  RCKStrobe(TRUE, IC10);
  RCKStrobe(TRUE, IC9);
}

BOOLEAN
getAMP_Relay(void) {
  return (myRig.m_IC7_Memory & AMP_RLYS) != 0;
}

void
setAMP_Relay(BOOLEAN value) {
  //Set or reset LNA relay
  if (value)
    myRig.m_IC7_Memory = (myRig.m_IC7_Memory | AMP_RLYS);
  else
    myRig.m_IC7_Memory = (myRig.m_IC7_Memory & (AMP_RLYS ^ 255));
  SRLoad(IC7, myRig.m_IC7_Memory);
}


BOOLEAN
getATTN_Relay(void) {
  return (myRig.m_IC7_Memory & ATTN_RLY) != 0;
}

void
setATTN_Relay(BOOLEAN value) {
  //Set or reset attenuator relay
  if (value)
    myRig.m_IC7_Memory = (myRig.m_IC7_Memory | ATTN_RLY);
  else
    myRig.m_IC7_Memory = (myRig.m_IC7_Memory & (ATTN_RLY ^ 255));
  SRLoad(IC7, myRig.m_IC7_Memory);
}

BOOLEAN
getXVTR_TR_Relay(void) {
  return (myRig.m_IC7_Memory & XVTR_TR_RLY) != 0;
}

void
setXVTR_TR_Relay(BOOLEAN value) {
  //Set or reset transverter relay
  if (value)
    myRig.m_IC7_Memory = (myRig.m_IC7_Memory | XVTR_TR_RLY);
  else
    myRig.m_IC7_Memory = myRig.m_IC7_Memory & (XVTR_TR_RLY ^ 255);
  SRLoad(IC7, myRig.m_IC7_Memory);
}

BOOLEAN
getXVTR_Relay(void) {
  return (myRig.m_IC7_Memory & XVTR_RLY) != 0;
}

void
setXVTR_Relay(BOOLEAN value) {
  //Set or reset XVTR 8R (TR) relay
  if (value)
    myRig.m_IC7_Memory = myRig.m_IC7_Memory | XVTR_RLY;
  else
    myRig.m_IC7_Memory = myRig.m_IC7_Memory & (XVTR_RLY ^ 255);
  SRLoad(IC7, myRig.m_IC7_Memory);
}

BOOLEAN
getIMPULSE_Relay(void) {
  return (myRig.m_IC7_Memory & IMPULSE_RLY) != 0;
}

void
setIMPULSE_Relay(BOOLEAN value) {
  //Set or reset Impulse relay
  if (value)
    myRig.m_IC7_Memory = myRig.m_IC7_Memory | IMPULSE_RLY;
  else
    myRig.m_IC7_Memory = myRig.m_IC7_Memory & (IMPULSE_RLY ^ 255);
  SRLoad(IC7, myRig.m_IC7_Memory);
}

void
Impulse(void) {
  //Send a single impulse to the QSD
  SRLoad(IC7, (myRig.m_IC7_Memory | 128));
  SRLoad(IC7, myRig.m_IC7_Memory);
}

BOOLEAN
openRig(char *port) {
  int mode = IEEE1284_MODE_COMPAT;
  if (!port) port = "/dev/parport0";
  if ((myRig.fd = open(port, O_RDWR)) < 0) {
    perror("open parallel port");
    return FALSE;
  }
  ioctl(myRig.fd, PPCLAIM);
  ioctl(myRig.fd, PPSETMODE, &mode);
  return TRUE;
}
  
void
closeRig(void) {
  ioctl(myRig.fd, PPRELEASE);
  close(myRig.fd);
}
