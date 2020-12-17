// hardware.c
// version 2

#include <hardware.h>

// Register memory for IC11
int ic11_memory;		

// Register memory for IC7
int ic7_memory;		

BOOL rfe_enabled;		// True if RFE board is enabled
BOOL xvtr_enabled;		// Transverter is enabled
BOOL pa_enabled;		// Transverter is enabled

// Latch Memories
BandSetting band_relay	= bs0;
int external_output	= 0;
int mute_relay		= 0;
int transmit_relay	= 0;
int gain_relay		= 0;
int latch_delay		= 0;	// measured in 100ns steps (10 = 1us)

// DDS Clock properties
double dds_clock		= 0.0;	// DDS Clock oscillator in MHz
int pll_mult			= 0;	// DDS PLL multiplier
double dds_clock_correction	= 0.0;	// Oscillator dds_clock_correction in MHz
double sysClock			= 0.0;	// DDS Internal System Clock in MHz
int ioud_clock			= 0;	// IO Update clock period
ushort dac_mult			= 0;	// DAC Multiplier setting

// DDS Frequency Control properties
double dds_freq		= 0.0;	// Last VFO Setting in MHz
double if_freq		= 0.0;	// IF Frequency Offset in MHz
BOOL if_shift		= FALSE;// Turns Offset Baseband IF Shift On
BOOL spur_reduction	= FALSE;// Eliminates Phase Truncation Spurs
double dds_step_size	= 0.0;	// DDS Step in MHz for spur removal
int sample_rate		= 0;	// ADC Sampling Rate in Hz
int fft_length		= 0;	// Length of FFT in bins
double FFT_Bin_Size	= 0.0;	// Bandwidth of FFT bin
int tune_fft		= 0;	// Offset tuning of FFT bins
double tune_frac_rel	= 0.0;	// In relative frequency (frequency/m_Sample_Rate)
double vfo_offset	= 0.0;
//double last_VFO	= 0.0;	// temp store for last VFO frequency

double min_freq = 0.012;	// minimum allowable tuning frequency
double max_freq = 65.0;		// maximum allowable tuning frequency

// PIO register base address
u_short baseAdr = 0x378;

// Current Bandplan
BandPlan curBandPlan = IARU1;

double TWO_TO_THE_48_DIVIDED_BY_200 = 1407374883553.28;
long last_tuning_word = 0;
// private Mutex parallel_mutex = new Mutex();
// private Mutex dataline_mutex = new Mutex();

BOOL usb_enabled = FALSE;

//------------------------------------------------------------------------

PRIVATE int parportfd = -1;

PUBLIC BOOL openPort(char *port) {
  int mode = IEEE1284_MODE_COMPAT;
  if (!port) port = "/dev/parport0";
  if ((parportfd = open(port, O_RDWR)) < 0) {
    perror("open parallel port");
    return FALSE;
  }
  ioctl(parportfd, PPCLAIM);
  ioctl(parportfd, PPSETMODE, &mode);
  return TRUE;
}
  
PUBLIC void closePort(void) {
  ioctl(parportfd, PPRELEASE);
  close(parportfd);
}

double DttSP_SampleRate = 48000.0;

PUBLIC void USB_Sdr1kLatch(int l, BYTE b) {}
PUBLIC BYTE USB_Sdr1kGetStatusPort(void) { return 0; }
PUBLIC int  USB_Sdr1kGetADC(void) { return 0; }
PUBLIC void USB_Sdr1kDDSReset(void) {}
PUBLIC void USB_Sdr1kDDSWrite(BYTE addr, BYTE data) {}
PUBLIC void USB_Sdr1kSRLoad(BYTE reg, BYTE data) {}
PUBLIC void DttSP_ChangeOsc(double val) {}

PRIVATE void Delay(void) {
  usleep(1000 * latch_delay);
}

PRIVATE void Sleep(int ms) {
  usleep(1000 * ms);
}

PRIVATE void PWrite(BYTE data) {
  //Write data Byte to parallel port
  ioctl(parportfd, PPWDATA, &data);
  //Delay to allow data line setup
  Delay();
}

PRIVATE void Latch(CtrlPin vCtrlPin) {
  //Strobe the specified pin to latch data
  unsigned char mask;
  switch (vCtrlPin) {
  case EXT:
    mask = 0xA, ioctl(parportfd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(parportfd, PPWCONTROL, &mask);
    break;
  case BPF:
    mask = 0x9, ioctl(parportfd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(parportfd, PPWCONTROL, &mask);
    break;
  case DAT:
    mask = 0xF, ioctl(parportfd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(parportfd, PPWCONTROL, &mask);
    break;
  case ADR:
    mask = 0x3, ioctl(parportfd, PPWCONTROL, &mask);
    Delay();
    mask = 0xB, ioctl(parportfd, PPWCONTROL, &mask);
    break;
  }
}

PRIVATE void ResetLatches(void)	{
  //Set all latch outputs to logic zero (relays off)
  if (usb_enabled) {
    gain_relay = GAIN;
    external_output = 0;
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, (BYTE) (external_output + gain_relay));
    setBandRelay(bsnone);
    transmit_relay = 0;
    mute_relay = MUTE;
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, (BYTE) (band_relay + transmit_relay + mute_relay));
  } else {
    PWrite(0);
    Latch(ADR);
    Latch(DAT);
    gain_relay = GAIN;
    external_output = 0;
    PWrite((BYTE) (external_output + gain_relay));
    Latch(EXT);
    setBandRelay(bsnone);
    transmit_relay = 0;
    mute_relay = MUTE;
    PWrite((BYTE) (band_relay + transmit_relay + mute_relay));
    Latch(BPF);
  }			
}

PRIVATE void DDSWrite(BYTE data, BYTE addr) {
  if (usb_enabled)
    USB_Sdr1kDDSWrite(addr, data);
  else {
    PWrite(data);
    Latch(DAT);
    PWrite((BYTE) (addr + WRB));
    Latch(ADR);
    PWrite(addr);
    Latch(ADR);
    PWrite(WRB);
    Latch(ADR);
  }
}

PRIVATE void ResetDDS(void) {
  if(usb_enabled)
    USB_Sdr1kDDSReset();
  else {
    PWrite(RESET + WRB);
    Latch(ADR);
    PWrite(WRB);
    Latch(ADR);
  }
  DDSWrite(COMP_PD, 29);		//Power down comparator
  if(pll_mult == 1)
    DDSWrite(BYPASS_PLL, 30);
  else
    DDSWrite((BYTE) pll_mult, 30);
  DDSWrite(BYPASS_SINC, 32);
}


PRIVATE void CalcClock(void) {
  sysClock = 200.0 + dds_clock_correction;
  dds_step_size = sysClock / pow(2.0, 48.0);
}

PRIVATE void SetRadioFreq(double f) {
  double vfoFreq = f;
  long tuning_word;

  // calculate software frequency to program
  if(xvtr_enabled && f >= 144 && f <= 146)
    f -= 116;									//Subtract 116MHz (144-28) from VFO display frequency

  if (if_shift)
    f -= if_freq;								// adjust for IF shift
  
  f += vfo_offset;								// adjust for vfo offset

  tuning_word = (long) (f / sysClock * pow(2.0, 48.0));

  if (spur_reduction) {
    // start with current tuning word
    // clear first bit, low 31 bits; set bit 31
    unsigned long long
      sr_tuning_word = (tuning_word & ~(0x80007fffffffLL)) | 0x000080000000LL;
    double software_offset = (sr_tuning_word - tuning_word) * dds_step_size;

    if (if_shift)								//Convert the tuning fraction to rel frq
      tune_frac_rel = 1000000.0 * (software_offset) - 11025.0;
    else
      tune_frac_rel = 1000000.0 * (software_offset);
    DttSP_ChangeOsc(tune_frac_rel);

    tuning_word = sr_tuning_word;
  }

  // program hardware
  SetBPF(vfoFreq);

  if (tuning_word != last_tuning_word) {
    int i;
    last_tuning_word = tuning_word;		   //save new tuning word    
				
    //parallel_mutex.WaitOne();
    for(i = 0; i < 6; i++) {
      BYTE b = (BYTE) (tuning_word >> (40 - i * 8));
      DDSWrite(b, (BYTE) (4 + i));
    }
    //parallel_mutex.ReleaseMutex();
  }
}

PUBLIC BOOL IsHamBand(BandPlan b) {
  if (getExtended()) return TRUE;
  switch (b) {
  case IARU1:
    if      (dds_freq >= 1.8 && dds_freq <= 2.0)	return TRUE;
    else if (dds_freq >= 3.5 && dds_freq <= 4.0)	return TRUE;
    else if (dds_freq == 5.3305)			return TRUE;
    else if (dds_freq == 5.3465)			return TRUE;
    else if (dds_freq == 5.3665)			return TRUE;
    else if (dds_freq == 5.3715)			return TRUE;
    else if (dds_freq == 5.4035)			return TRUE;
    else if (dds_freq >= 7.0 && dds_freq <= 7.3)	return TRUE;
    else if (dds_freq >= 10.1 && dds_freq <= 10.15) 	return TRUE;
    else if (dds_freq >= 14.0 && dds_freq <= 14.35)	return TRUE;
    else if (dds_freq >= 18.068 && dds_freq <= 18.168)	return TRUE;
    else if (dds_freq >= 21.0 && dds_freq <= 21.45)	return TRUE;
    else if (dds_freq >= 24.89 && dds_freq <= 24.99) 	return TRUE;
    else if (dds_freq >= 21.0 && dds_freq <= 21.45)	return TRUE;
    else if (dds_freq >= 28.0 && dds_freq <= 29.7)	return TRUE;
    else if (dds_freq >= 50.0 && dds_freq <= 54.0)	return TRUE;
    else if (dds_freq >= 144.0 && dds_freq <= 146.0) {
      if (rfe_enabled && xvtr_enabled) return TRUE;
      else                             return FALSE;
    } else return FALSE;
    default:
      return FALSE;
      // TODO: Implement other bandplans here
  }
}

//------------------------------------------------------------------------
// Properties
//

PRIVATE BOOL enable_LPF0 = FALSE;
PUBLIC  BOOL getEnableLPF0       (void) { return enable_LPF0; }
PUBLIC  void setEnableLPF0 (BOOL value) { enable_LPF0 = value; };

PRIVATE BOOL extended = FALSE;
PUBLIC  BOOL getExtended       (void) { return extended; }
PUBLIC  void setExtended (BOOL value) { extended = value; }

PRIVATE BOOL x2_enabled = FALSE;
PUBLIC  BOOL getX2Enabled       (void) { return x2_enabled; }
PUBLIC  void setX2Enabled (BOOL value) { x2_enabled = value; }

PRIVATE int  x2_delay = 500;
PUBLIC  int  getX2Delay      (void) { return x2_delay; }
PUBLIC  void setX2Delay (int value) { x2_delay = value; }

PUBLIC BOOL getRFE_Enabled       (void) { return rfe_enabled; }
PUBLIC void setRFE_Enabled (BOOL value) {
  rfe_enabled = value;
  SetRadioFreq(dds_freq);
}

PUBLIC BOOL getPA_Enabled       (void) { return pa_enabled; }
PUBLIC void setPA_Enabled (BOOL value) { pa_enabled = value; }

PUBLIC BOOL getXVTR_Enabled       (void) { return xvtr_enabled; }
PUBLIC BOOL setXVTR_Enabled (BOOL value) {
  xvtr_enabled = value;
  if (xvtr_enabled) max_freq = 146.0;
  else              max_freq = 65.0;
}

PUBLIC BOOL getUSB_Enabled       (void) { return usb_enabled; }
PUBLIC void setUSB_Enabled (BOOL value) { usb_enabled = value; }

PRIVATE XVTRTRMode current_xvtr_tr_mode = NEGATIVE;
PUBLIC  XVTRTRMode getCurrentXVTRTRMode             (void) { return current_xvtr_tr_mode; }
PUBLIC  void       setCurrentXVTRTRMode (XVTRTRMode value) {
  current_xvtr_tr_mode = value;
  switch (current_xvtr_tr_mode) {
  case NEGATIVE: setXVTR_TR_Relay(TRUE);  break;	// Set to receive
  case POSITIVE: setXVTR_TR_Relay(FALSE); break;	// Set to receive
  case NONE:                              break;
  }
}

PUBLIC int  getLatchDelay      (void) { return latch_delay; }
PUBLIC void setLatchDelay (int value) { latch_delay = value; }

PUBLIC double getMinFreq (void) { return min_freq; }
PUBLIC double getMaxFreq (void) { return max_freq; }

PUBLIC u_short getBaseAddr          (void) { return baseAdr; }
PUBLIC u_short setBaseAddr (u_short value) { baseAdr = value; }

PUBLIC BandSetting getBandRelay              (void) { return band_relay; }
PUBLIC void        setBandRelay (BandSetting value) {
  band_relay = value;
  if (usb_enabled)
    USB_Sdr1kLatch(SDR1K_LATCH_BPF,
		   (BYTE) (band_relay + transmit_relay + mute_relay));
  else {
    PWrite((BYTE) (band_relay + transmit_relay + mute_relay));
    Latch(BPF);
  }
}

PUBLIC BOOL getTransmitRelay (void) {
  //Get state of TR relay on BPF board
  if (transmit_relay == TR) return TRUE;
  else                      return FALSE;
}
PUBLIC void setTransmitRelay (BOOL value) {
  BYTE tmpLatch = 0;
  BOOL tmpATTN = FALSE;
  //If in valid Amateur BandRelay Save and output new TR Relay setting
  if (value == TRUE) {
    //if (IsHamBand(curBandPlan) == TRUE)
    {
      if (x2_enabled) {
	SetExt(P7);		// set X2-7
	Sleep(x2_delay);	// pause for ext. relays
      }
      transmit_relay = TR;	// Set to TX
      if (rfe_enabled == TRUE) {
	tmpATTN = getATTN_Relay();
	if (xvtr_enabled && dds_freq >= 144) {
	  switch (current_xvtr_tr_mode) {
	  case NEGATIVE: setXVTR_TR_Relay(FALSE); break;
	  case POSITIVE: setXVTR_TR_Relay(TRUE);  break;
	  case NONE:                              break;
	  }
	} else setAMP_Relay(TRUE);					//Switch RFE to transmit
	tmpLatch = (BYTE) (transmit_relay + mute_relay + DCDR_NE);// DCDR_NE for 2-4 Decoder to stay high
      }	else
	tmpLatch = (BYTE) (band_relay + transmit_relay + mute_relay);
    }
  } else {
    transmit_relay = 0;       // Set to RX
    if (rfe_enabled) {
      setAMP_Relay(FALSE);
      tmpLatch = (BYTE) (transmit_relay + mute_relay + DCDR_NE);
      if (xvtr_enabled) {
	switch(current_xvtr_tr_mode) {
	case NEGATIVE: setXVTR_TR_Relay(TRUE);  break;
	case POSITIVE: setXVTR_TR_Relay(FALSE); break;
	case NONE:                              break;
	}
      }
      setATTN_Relay(tmpATTN);
    } else
      tmpLatch = (BYTE) (band_relay + transmit_relay + mute_relay);
  }
  if (usb_enabled)
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, tmpLatch);
  else {
    PWrite(tmpLatch);
    Latch(BPF);
  }
  if (!value && x2_enabled) {
    Sleep(x2_delay);	// pause for ext. relays
    ResExt(P7);		// clear X2-7 
  }
  if (pa_enabled) {	// set PA transmit/bias
    if (dds_freq >= 1.8 && dds_freq <= 29.7) {
      if (value) {	// TX
	setPA_TransmitRelay(TRUE);
	setPA_BiasOn(TRUE);
      } else {		// RX
	setPA_BiasOn(FALSE);
	setPA_TransmitRelay(FALSE);
      }
    }
  }
}

PUBLIC BOOL getMuteRelay (void) {
  // Get state of MUTE relay on TRX board
  if (mute_relay == MUTE) return FALSE;
  else                    return TRUE;
}
PUBLIC void setMuteRelay (BOOL value) {
  BYTE data = 0;
  // Mute the speaker relay if TRUE
  if (value == TRUE) mute_relay = 0;
  else               mute_relay = MUTE;
  if (rfe_enabled)
    data = (BYTE) (transmit_relay + mute_relay + DCDR_NE);
  else
    data = (BYTE) (band_relay + transmit_relay + mute_relay);
  if (usb_enabled) {
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, data);
  } else {
    PWrite(data);
    Latch(BPF);
  }
}

PUBLIC BOOL getGainRelay (void) {
  // Get state of GAIN relay on TRX board
  if (gain_relay == GAIN) return FALSE;
  else                    return TRUE;
}
PUBLIC void setGainRelay (BOOL value) {
  // Save and output state of GAIN relay on TRX board
  if (value == TRUE) gain_relay = 0;	// 40dB or 0dB w/RFE
  else               gain_relay = GAIN;	// 26dB
  if (usb_enabled)
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, (BYTE) (external_output + gain_relay));
  else {
    PWrite((BYTE) (external_output + gain_relay));
    Latch(EXT);
  }
}

PUBLIC int  getExternalOutput      (void) { return external_output; }
PUBLIC void setExternalOutput (int value) {
  //Save and output state of External Control outputs on PIO board
  external_output = value;
  if (usb_enabled)
      USB_Sdr1kLatch(SDR1K_LATCH_EXT, (BYTE) (external_output + gain_relay));
  else {
    PWrite((BYTE) (external_output + gain_relay));
    Latch(EXT);
  }
}

PUBLIC double getDDSClockCorrection         (void) { return dds_clock_correction; }
PUBLIC void   setDDSClockCorrection (double value) {
  dds_clock_correction = value;
  CalcClock();
  SetRadioFreq(dds_freq);
}

PUBLIC int  getPLLMult      (void) { return pll_mult; }
PUBLIC void setPLLMult (int value) {
  pll_mult = value;
  if (pll_mult == 1) DDSWrite(BYPASS_PLL, 30);	    // Bypass PLL if multiplier value is 1
  else               DDSWrite((BYTE) pll_mult, 30); // Set for External Clock
  CalcClock();
}

PUBLIC double getDDSClock         (void) { return dds_clock; }
PUBLIC void   setDDSClock (double value) {
  //Compute internal DDS System Clock and Phase Truncation Elimination Step
  dds_clock = value;
  CalcClock();
}

PUBLIC BOOL getIFShift       (void) { return if_shift; }
PUBLIC void setIFShift (BOOL value) {
  // Turns IF shift on and off
  if_shift = value;
  if (!spur_reduction) {
    if (if_shift) DttSP_ChangeOsc(-11025.0);
    else          DttSP_ChangeOsc(0.0);
  }
  SetRadioFreq(dds_freq);
}

PUBLIC BOOL getSpurReduction       (void) { return spur_reduction; }
PUBLIC BOOL setSpurReduction (BOOL value) {
  // Turns DDS Phase Truncation Spur reduction on and off
  spur_reduction = value;
  if (!spur_reduction) {
      if (if_shift) DttSP_ChangeOsc(-11025.0);
      else          DttSP_ChangeOsc(0.0);
  }
  SetRadioFreq(dds_freq);
}

PUBLIC double getIFFreq         (void) { return if_freq; }
PUBLIC void   setIFFreq (double value) { if_freq = value; }

PUBLIC double getDDSFreq         (void) { return dds_freq; }
PUBLIC void   setDDSFreq (double value) {
  dds_freq = value;
  SetRadioFreq(dds_freq);
}

PUBLIC int  getSampleRate      (void) { return sample_rate; }
PUBLIC void setSampleRate (int value) {
  sample_rate = value;
  if (fft_length > 0)
    FFT_Bin_Size = (sample_rate / fft_length) * 1e-6;
}

PUBLIC int  getFFTLength      (void) { return fft_length; }
PUBLIC void setFFTLength (int value) {
  fft_length = value;
  //Compute bandwidth of FFT bin
  if (fft_length > 0)
    FFT_Bin_Size = (sample_rate / fft_length) * 1e-6;
}

PUBLIC int getTuneFFT (void) { return tune_fft; }

PUBLIC double getTuneFracRel (void) { return tune_frac_rel; }

PUBLIC double getVFOOffset         (void) { return vfo_offset; }
PUBLIC void   setVFOOffset (double value) {
  vfo_offset = value;
  SetRadioFreq(dds_freq);
}

PUBLIC int  getIOUDClock      (void) { return ioud_clock; }
PUBLIC void setIOUDClock (int value) {
  double bitVal, bytVal;
  BYTE lWord;
  int i;
  ioud_clock = value;			//Save value
  bitVal = value;			//Compute Numeric Value
  for (i = 24; i >= 0; i -= 8) {		//Convert to binary strings
      bytVal = bitVal / (01 << i);	//Compute binary byte Value
      lWord = (BYTE) bytVal;		//Truncate fractional portion
      bitVal -= lWord * (01 << i);	//Reduce value
      switch (i) {			//Write to byte position
	case 32: DDSWrite(lWord, 22); break;
	case 16: DDSWrite(lWord, 23); break;
	case 8:  DDSWrite(lWord, 24); break;
	case 0:  DDSWrite(lWord, 25); break;
	}
    }
}

PUBLIC u_short getDACMult          (void) { return dac_mult; }
PUBLIC void    setDACMult (u_short value) {
  double bitVal, bytVal;
  BYTE lWord;
  int i;
  dac_mult = value;
  bitVal = value;			//Compute Numeric Value
  for (i = 8; i >= 0; i -= 8) {		//Convert to binary strings
    bytVal = bitVal / (01 << i);	//Compute binary byte Value
    BYTE lWord = (BYTE) bytVal;	//Truncate fractional portion
    bitVal -= lWord * (01 << i);	//Reduce value
    switch (i) {
    case 8: DDSWrite(lWord, 33); break;
    case 0: DDSWrite(lWord, 34); break;
    }
  }
  //Send new I DAC Multiplier value to DDS
  bitVal = value;			//Compute Numeric Value
  for (i = 8; i >= 0; i -= 8) {		//Convert to binary strings.
    bytVal = bitVal / (01 << i);	//Compute binary byte Value
    lWord = (BYTE) bytVal;          	//Truncate fractional portion
    bitVal -= lWord * (01 << i);	//Reduce value
      switch (i) {			//Write to byte position
	case 8: DDSWrite(lWord, 35); break;
	case 0: DDSWrite(lWord, 36); break;
      }
  }
}

PUBLIC BYTE StatusPort(void) {	
  if (usb_enabled)
    return (BYTE) USB_Sdr1kGetStatusPort();
  else {
    BYTE status;
    ioctl(parportfd, PPRSTATUS, (char *) &status);
    return status;
  }
}

PUBLIC void Init(void) {
  setIFShift(TRUE);
  // user setable through the setup form
  // DDSClockCorrection = 0.000;
  setPLLMult(1);
  setDDSClock(200);
  setIFFreq(0.011025);
  setSampleRate((int) DttSP_SampleRate);
  setFFTLength(4096);
  //setDACMult(4095);
}

PUBLIC void PowerOn(void) {
  if (rfe_enabled) {
    // set mute/gain relays based on console
    if (xvtr_enabled && current_xvtr_tr_mode == NEGATIVE)
      setXVTR_TR_Relay(TRUE);
    else
      setXVTR_TR_Relay(FALSE);
    //DDSFreq = dds_freq;
    ic11_memory |= ADC_CS_NOT;		// set CS_NOT high
    ic11_memory |= ATUCTL;
    ic7_memory  |= PA_BIAS_NOT;
    SRLoad(IC11, ic11_memory);				
    SRLoad(IC7,  ic7_memory);
  }
}

PUBLIC void StandBy(void) {
  ResetLatches();
  if (rfe_enabled) ResetRFE();
  if (xvtr_enabled) setXVTR_TR_Relay(FALSE);
  // TODO: Fix bias glitch on reset w/hardware rewrite
  if (rfe_enabled) {
      SRLoad(IC7, PA_BIAS_NOT);
      SRLoad(IC11, ADC_CS_NOT);
  }
  ResetDDS();
}

PUBLIC void SetExt(ExtPin pin) {
  //Set the designated external pin high
  external_output |= (BYTE) pin;
  if (usb_enabled)
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, (BYTE) (external_output + gain_relay));
  else {
    PWrite((BYTE) (external_output + gain_relay));
    Latch(EXT);
  }
}

PUBLIC void ResExt(ExtPin pin) {
  //Reset the designated external pin high
  external_output &= ~(BYTE) pin;
  if (usb_enabled)
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, (BYTE) (external_output + gain_relay));
  else {
    PWrite((BYTE) (external_output + gain_relay));
    Latch(EXT);
  }
}

PUBLIC BOOL PinValue(ExtPin pin) {
  //Return TRUE if Pin is set
  if ((external_output & (int) pin) != 0)
    return TRUE;
  else
    return FALSE;
}

PUBLIC void SetBPF(double vfo_value) {
  if (rfe_enabled) {
      if (vfo_value <= 2.5) {
	if (pa_enabled) PA_SetLPF(PA_LPF_160);
	if (vfo_value <= 0.3 && enable_LPF0) {
	  SRLoad(IC10, BPF0);
	  SRLoad(IC9, LPF0);						
	} else {
	  SRLoad(IC10, LPF9 + BPF0);
	  SRLoad(IC9, 0);
	}
      } else if (vfo_value <= 4) {
	if (pa_enabled) PA_SetLPF(PA_LPF_80);
	SRLoad(IC10, BPF1);
	SRLoad(IC9,  LPF7);
      } else if (vfo_value <= 6) {
	if (pa_enabled) PA_SetLPF(PA_LPF_60_40);
	SRLoad(IC10, BPF1);
	SRLoad(IC9,  LPF2);
      } else if (vfo_value <= 7.3) {
	if (pa_enabled) PA_SetLPF(PA_LPF_60_40);
	SRLoad(IC10, BPF2);
	SRLoad(IC9,  LPF5);
      } else if (vfo_value <= 10.2) {
	if (pa_enabled) PA_SetLPF(PA_LPF_30_20);
	SRLoad(IC10, BPF2);
	SRLoad(IC9,  LPF4);
      } else if (vfo_value <= 12) {
	if (pa_enabled) PA_SetLPF(PA_LPF_OFF);
	SRLoad(IC10, BPF2);
	SRLoad(IC9,  LPF3);
      } else if (vfo_value <= 14.5) {
	if (pa_enabled) PA_SetLPF(PA_LPF_30_20);
	SRLoad(IC10, BPF3);
	SRLoad(IC9,  LPF3);
      } else if (vfo_value <= 21.5) {
	if (pa_enabled) PA_SetLPF(PA_LPF_17_15);
	SRLoad(IC10, BPF3 + LPF8);
	SRLoad(IC9,  0);
      } else if (vfo_value <= 24) {
	if (pa_enabled) PA_SetLPF(PA_LPF_12_10);
	SRLoad(IC10, BPF3);
	SRLoad(IC9,  LPF6);
      } else if (vfo_value <= 30) {
	if (pa_enabled) PA_SetLPF(PA_LPF_12_10);
	SRLoad(IC10, BPF4);
	SRLoad(IC9,  LPF6);
      } else if (vfo_value <= 36) {
	if (pa_enabled) PA_SetLPF(PA_LPF_OFF);
	SRLoad(IC10, BPF4);
	SRLoad(IC9,  LPF1);
      } else if (vfo_value <= 65) {
	if (pa_enabled) PA_SetLPF(PA_LPF_OFF);
	SRLoad(IC10, BPF5);
	SRLoad(IC9,  LPF1);
      } else if (xvtr_enabled && vfo_value >= 144 && vfo_value <= 146) {
	if (pa_enabled) PA_SetLPF(PA_LPF_OFF);
	SRLoad(IC10, BPF4);
	SRLoad(IC9,  LPF6);
	setXVTR_Relay(TRUE);
      }
      if (xvtr_enabled && vfo_value < 144)
	setXVTR_Relay(FALSE);
    } else {
      if      (vfo_value < 2.5)	setBandRelay(bs0);
      else if (vfo_value < 6)	setBandRelay(bs1);
      else if (vfo_value < 12)	setBandRelay(bs2);
      else if (vfo_value < 24)	setBandRelay(bs3);
      else if (vfo_value < 36)	setBandRelay(bs4);
      else			setBandRelay(bs5);
    }
}

PUBLIC void TestPort(void) {
  //Toggle 1 and 0 to each of the four parallel port latches
  if (usb_enabled) {
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, 0);
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, 0);
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, 255);
    USB_Sdr1kLatch(SDR1K_LATCH_EXT, 255);
  } else {
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
}

PUBLIC void RCKStrobe(BOOL ClearReg, RFE_RCK Reg) {
  // Strobe the RFE 1:4 decoder output to transfer contents
  // of shift register to output latches
  BYTE data = 0;
  if (ClearReg) data = (BYTE) (Reg);
  else data = (BYTE) (SCLR_NOT + Reg + transmit_relay + mute_relay);
  if (usb_enabled) {
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, data);
    USB_Sdr1kLatch(SDR1K_LATCH_BPF, (BYTE) (SCLR_NOT + DCDR_NE + transmit_relay + mute_relay));
  } else {
    PWrite(data);
    Latch(BPF);
    PWrite((BYTE) (SCLR_NOT + DCDR_NE + transmit_relay + mute_relay));
    Latch(BPF);
  }
}

PUBLIC void SRLoad(RFE_RCK Reg, int Data) {
  if (usb_enabled)
    USB_Sdr1kSRLoad((BYTE) Reg, (BYTE) Data);
  else {
    static int choose[8] = {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01};
    int i;
    //Shift data into registers on RFE
    //parallel_mutex.WaitOne();
    for (i = 0; i < 8; i++) {
      int mask = choose[i];												// Mask the current bit
      if ((mask & Data) == 0) {
	PWrite((BYTE) (SCLR_NOT + DCDR_NE + transmit_relay + mute_relay));
	Latch(BPF);
	PWrite((BYTE) (SCLR_NOT + DCDR_NE + SCK + transmit_relay + mute_relay));
      } else {																// Current bit = 1	    {
	PWrite((BYTE) (SCLR_NOT + DCDR_NE + SER + transmit_relay + mute_relay));
	Latch(BPF);
	PWrite((BYTE) (SCLR_NOT + DCDR_NE + SER + SCK + transmit_relay + mute_relay));
      }
      Latch(BPF);
      PWrite((BYTE) (SCLR_NOT + DCDR_NE + transmit_relay + mute_relay));
      Latch(BPF);
    }
    RCKStrobe(FALSE, Reg);													// Strobe Register Clock
    //parallel_mutex.ReleaseMutex();
  }
}

PUBLIC void ResetRFE(void) {
  //Reset all RFE shift registers to zero output
  //parallel_mutex.WaitOne();
  RCKStrobe(TRUE, IC11);
  RCKStrobe(TRUE, IC7);
  RCKStrobe(TRUE, IC10);
  RCKStrobe(TRUE, IC9);
  //parallel_mutex.ReleaseMutex();
}

PUBLIC BOOL getAMP_Relay       (void) { return (ic7_memory & AMP_RLYS) != 0; }
PUBLIC BOOL setAMP_Relay (BOOL value) {
  //Set or reset LNA relay
  if (value)  ic7_memory |= AMP_RLYS;
  else        ic7_memory &= ~(AMP_RLYS);
  SRLoad(IC7, ic7_memory);
}

PUBLIC BOOL getATTN_Relay       (void) { return (ic7_memory & ATTN_RLY) != 0; }
PUBLIC void setATTN_Relay (BOOL value) {
  if (value)  ic7_memory |= ATTN_RLY;
  else        ic7_memory &= ~(ATTN_RLY);
  SRLoad(IC7, ic7_memory);
}

PUBLIC BOOL getXVTR_TR_Relay       (void) { return (ic7_memory & XVTR_TR_RLY) != 0; }
PUBLIC BOOL setXVTR_TR_Relay (BOOL value) {
  if (value)  ic7_memory |= XVTR_TR_RLY;
  else        ic7_memory &= ~(XVTR_TR_RLY);
  SRLoad(IC7, ic7_memory);
}

PUBLIC BOOL getXVTR_Relay       (void) { return (ic7_memory & XVTR_RLY) != 0; }
PUBLIC BOOL setXVTR_Relay (BOOL value) {
  if (value)  ic7_memory |= XVTR_RLY;
  else        ic7_memory &= ~(XVTR_RLY);
  SRLoad(IC7, ic7_memory);
}

PUBLIC BOOL getIMPULSE_Relay       (void) { return (ic7_memory & IMPULSE_RLY) != 0; }
PUBLIC BOOL setIMPULSE_Relay (BOOL value) {
  if (value)  ic7_memory |= IMPULSE_RLY;
  else        ic7_memory &= ~(IMPULSE_RLY);
  SRLoad(IC7, ic7_memory);
}

PUBLIC void Impulse(void) {
  //Send a single impulse to the QSD
  SRLoad(IC7, (ic7_memory | IMPULSE));
  SRLoad(IC7, ic7_memory);
}

PUBLIC void PA_SetLPF(int i) {
  int temp = ic11_memory;
  switch(i) {
    case PA_LPF_OFF:	// 0
      ic11_memory &= ~(0x07);				// clear bits 0, 1 and 2
      break;
    case PA_LPF_12_10:	// 1
      ic11_memory  = (ic11_memory | 0x01) & ~(0x06);	// set bit 0, clear bits 1,2
      break;
    case PA_LPF_17_15:	// 2
      ic11_memory  = (ic11_memory | 0x02) & ~(0x05);	// set bit 1, clear bits 0,2
      break;
    case PA_LPF_30_20:	// 3
      ic11_memory  = (ic11_memory | 0x03) & ~(0x04);	// set bits 0,1, clear bit 2
      break;
    case PA_LPF_60_40:	// 4
      ic11_memory  = (ic11_memory | 0x04) & ~(0x03);	// set bit 2, clear bits 0,1
      break;
    case PA_LPF_80:	// 5
      ic11_memory  = (ic11_memory | 0x05) & ~(0x02);	// set bits 0,2, clear bit 1
      break;
    case PA_LPF_160:	// 6
      ic11_memory  = (ic11_memory | 0x06) & ~(0x01);	// set bits 1,2, clear bit 0
      break;
    }
  if (temp != ic11_memory)
    SRLoad(IC11, ic11_memory);
}

PUBLIC BOOL getPA_TransmitRelay       (void) { return ((ic11_memory & PATR) != 0); }
PUBLIC BOOL setPA_TransmitRelay (BOOL value) {
  int temp = ic11_memory;
  if (value) ic11_memory |= PATR;
  else       ic11_memory &= ~(PATR);
  if (temp != ic11_memory)
    SRLoad(IC11, ic11_memory);
}

PUBLIC BOOL getPA_BiasOn       (void) { return ((ic7_memory & PA_BIAS_NOT) != 0); }
PUBLIC BOOL setPA_BiasOn (BOOL value) {
  int temp = ic7_memory;
  if (value) ic7_memory &= ~(PA_BIAS_NOT);
  else       ic7_memory |= PA_BIAS_NOT;
  if (temp != ic7_memory)
    SRLoad(IC7, ic7_memory);
}

PUBLIC BYTE PA_GetADC(int chan)	{
  short num = 0;
  int i;
  // get ADC on amplifier
  // 0 for forward power, 1 for reverse

  if (usb_enabled) {
    int data = USB_Sdr1kGetADC();
    if (chan == 0) return (BYTE) (data & 255);
    else           return (BYTE) (data >> 8);
  }
  
  //dataline_mutex.WaitOne();
  //parallel_mutex.WaitOne();
  
  ic11_memory &= ~(ADC_CS_NOT);		// CS not goes low
  SRLoad(IC11, ic11_memory);
  
  ic11_memory |= ADC_DI;				// set DI bit high for start bit
  SRLoad(IC11, ic11_memory);
  
  ic11_memory |= ADC_CLK;				// clock it into shift register
  SRLoad(IC11, ic11_memory);
  ic11_memory &= ~(ADC_CLK);
  SRLoad(IC11, ic11_memory);
  
  // set DI bit high for single ended -- done since DI is already high
  ic11_memory |= ADC_CLK;				// clock it into shift register
  SRLoad(IC11, ic11_memory);
  ic11_memory &= ~(ADC_CLK);
  SRLoad(IC11, ic11_memory);
  
  if (chan == PA_FORWARD_PWR) {
    ic11_memory &= ~(ADC_DI);		// set DI bit low for Channel 0
    SRLoad(IC11, ic11_memory);
  } else {
    // set DI bit high for Channel 1 -- done since DI is already high
  }
  
  ic11_memory |= ADC_CLK;		// clock it into shift register
  SRLoad(IC11, ic11_memory);
  ic11_memory &= ~(ADC_CLK);
  SRLoad(IC11, ic11_memory);
  
  for(i = 0; i < 8; i++) {		// read 15 bits out of DO
    ic11_memory |= ADC_CLK;			// clock high
    SRLoad(IC11, ic11_memory);
    ic11_memory &= ~(ADC_CLK);		// clock low
    SRLoad(IC11, ic11_memory);
      
    if ((StatusPort() & (BYTE) PA_DATA) != 0)	// read DO 
      num++;	// add bit		
      
    if (i != 7) num <<= 1;
  }
  
  ic11_memory |= ADC_CS_NOT;		// CS not goes high
  SRLoad(IC11, ic11_memory);
  
  //dataline_mutex.ReleaseMutex();
  //parallel_mutex.ReleaseMutex();
  
  return (BYTE) (num);
}

PUBLIC BOOL PA_ATUTune(ATUTuneMode mode) {
  int count = 0, delay = 0;

  //dataline_mutex.WaitOne();

  ic11_memory &= ~(ATUCTL);

  SRLoad(IC11, ic11_memory);
  switch (mode) {
  case BYPASS: delay =  250; break;
  case MEMORY: delay = 2000; break;
  case FULL:   delay = 3250; break;
  }
  
  Sleep(delay);
  ic11_memory |= ATUCTL;
  SRLoad(IC11, ic11_memory);
  
  if (mode == MEMORY || mode == FULL) {
    while ((StatusPort() & (BYTE) PA_DATA) != 0)	{ // wait for low output from ATU
      Sleep(50);
      if (count++ > 100) return FALSE;
    }
    count = 0;
    while ((StatusPort() & (BYTE) PA_DATA) == 0)	{ // wait for high output from ATU
      Sleep(50);
      if (count++ > 100) return FALSE;
    }
    Sleep(1000);
  }
  
  //dataline_mutex.ReleaseMutex();
  
  return TRUE;
}
