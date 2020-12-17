/* update.c

common defs and code for parm update 
   
This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2004 by Frank Brickle, AB2KT and Bob McGwier, N4HY

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

The authors can be reached by email at

ab2kt@arrl.net
or
rwmcgwier@comcast.net

or by paper mail at

The DTTS Microwave Society
6 Kathleen Place
Bridgewater, NJ 08807
*/

#include <common.h>

////////////////////////////////////////////////////////////////////////////

PRIVATE REAL
dB2lin(REAL dB) { return pow(10.0, dB / 20.0); }


PRIVATE int
setRXFilter(int n, char **p) {
  REAL low_frequency  = atof(p[0]),
       high_frequency = atof(p[1]);
  int ncoef = uni.buflen + 1;
  int i, fftlen = 2 * uni.buflen;
  fftw_plan ptmp;
  COMPLEX *zcvec;

  if (fabs(low_frequency) >= 0.5 * uni.samplerate) return -1;
  if (fabs(high_frequency) >= 0.5 * uni.samplerate) return -2;
  if ((low_frequency + 10) >= high_frequency) return -3;
  delFIR_COMPLEX(rx.filt.coef);

  rx.filt.coef = newFIR_Bandpass_COMPLEX(low_frequency,
					 high_frequency,
					 uni.samplerate,
					 ncoef);

  zcvec = newvec_COMPLEX(fftlen, "filter z vec in setFilter");
  ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, FFTW_OUT_OF_PLACE);
#ifdef LHS
  for (i = 0; i < ncoef; i++) zcvec[i] = rx.filt.coef->coef[i];
#else
  for (i = 0; i < ncoef; i++) zcvec[fftlen - ncoef + i] = rx.filt.coef->coef[i];
#endif
  fftw_one(ptmp, (fftw_complex *) zcvec, (fftw_complex *) rx.filt.ovsv->zfvec);
  fftw_destroy_plan(ptmp);
  delvec_COMPLEX(zcvec);
  memcpy((char *) rx.filt.save,
	 (char *) rx.filt.ovsv->zfvec,
	 rx.filt.ovsv->fftlen * sizeof(COMPLEX));

  return 0;
}


PRIVATE int
setTXFilter(int n, char **p) {
  REAL low_frequency  = atof(p[0]),
       high_frequency = atof(p[1]);
  int ncoef = uni.buflen + 1;
  int i, fftlen = 2 * uni.buflen;
  fftw_plan ptmp;
  COMPLEX *zcvec;

  if (fabs(low_frequency) >= 0.5 * uni.samplerate) return -1;
  if (fabs(high_frequency) >= 0.5 * uni.samplerate) return -2;
  if ((low_frequency + 10) >= high_frequency) return -3;
  delFIR_COMPLEX(tx.filt.coef);
  tx.filt.coef = newFIR_Bandpass_COMPLEX(low_frequency,
					high_frequency,
					uni.samplerate,
					ncoef);
  
  zcvec = newvec_COMPLEX(fftlen, "filter z vec in setFilter");
  ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, FFTW_OUT_OF_PLACE);
#ifdef LHS
  for (i = 0; i < ncoef; i++) zcvec[i] = tx.filt.coef->coef[i];
#else
  for (i = 0; i < ncoef; i++) zcvec[fftlen - ncoef + i] = tx.filt.coef->coef[i];
#endif
  fftw_one(ptmp, (fftw_complex *) zcvec, (fftw_complex *) tx.filt.ovsv->zfvec);
  fftw_destroy_plan(ptmp);
  delvec_COMPLEX(zcvec);
  memcpy((char *) tx.filt.save,
	 (char *) tx.filt.ovsv->zfvec,
	 tx.filt.ovsv->fftlen * sizeof(COMPLEX));

  return 0;
}

PRIVATE int
setFilter(int n, char **p) {
  if (n == 2) return setRXFilter(n, p);
  else {
    int trx = atoi(p[2]);
    if      (trx == RX) return setRXFilter(n, p);
    else if (trx == TX) return setTXFilter(n, p);
    else                return -1;
  }
}

// setMode <mode> [TRX]

PRIVATE int
setMode(int n, char **p) {
  int mode = atoi(p[0]);
  if (n > 1) {
    int trx = atoi(p[1]);
    switch (trx) {
    case TX: tx.mode = mode; break;
    case RX: 
    default: rx.mode = mode; break;
    }
  } else
    tx.mode = rx.mode = uni.mode.sdr = mode;
  if (rx.mode == AM) rx.am.gen->mode = AMdet;
  if (rx.mode == SAM) rx.am.gen->mode = SAMdet;
  return 0;
}

// setOsc <freq> [TRX]
PRIVATE int
setOsc(int n, char **p) {
  REAL newfreq = atof(p[0]);
//  fprintf(stderr,"freq =%lf, samplerate = %lf\n",newfreq,uni.samplerate);
  if (fabs(newfreq) >= 0.5 * uni.samplerate) return -1;
  newfreq *= 2.0 * M_PI / uni.samplerate;
  if (n > 1) {
    int trx = atoi(p[1]);
    switch (trx) {
    case TX: tx.osc.gen->Frequency = newfreq; break;
    case RX:
    default: rx.osc.gen->Frequency = newfreq; break;
    }
  } else
    tx.osc.gen->Frequency = rx.osc.gen->Frequency = newfreq;
  return 0;
}

PRIVATE int
setSampleRate(int n, char **p) {
  REAL samplerate = atof(p[0]);
  uni.samplerate = samplerate;
  return 0;
}

PRIVATE int
setNR(int n, char **p) {
  rx.anr.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setANF(int n, char **p) {
  rx.anf.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setNB(int n, char **p) {
  rx.nb.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setNBvals(int n, char **p) {
 REAL threshold = atof(p[0]);
  rx.nb.gen->threshold = rx.nb.thresh = threshold;
  return 0;
}

PRIVATE int
setSDROM(int n, char **p) {
  rx.nb_sdrom.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setSDROMvals(int n, char **p) {
 REAL threshold = atof(p[0]);
  rx.nb_sdrom.gen->threshold = rx.nb_sdrom.thresh = threshold;
  return 0;
}

PRIVATE int
setBIN(int n, char **p) {
  rx.bin.flag = atoi(p[0]);
  return 0;
}

PRIVATE
// setfixedAGC <gain> [TRX]
int
setfixedAGC(int n, char **p) {
  REAL gain = atof(p[0]);
  if (n > 1) {
    int trx = atoi(p[1]);
    switch(trx) {
    case TX: tx.agc.gen->AgcFixedGain = gain; break;
    case RX:
    default: rx.agc.gen->AgcFixedGain = gain; break;
    }
  } else
    tx.agc.gen->AgcFixedGain = rx.agc.gen->AgcFixedGain = gain;
  return 0;
}

PRIVATE int
setTXAGC(int n, char **p) {
  int setit = atoi(p[0]);
  switch (setit) {
  case agcOFF:
    tx.agc.gen->AgcMode = agcOFF;
    tx.agc.flag = FALSE;
    break;
  case agcSLOW:
    tx.agc.gen->AgcMode = agcSLOW;
    tx.agc.gen->Hang = 10;
    tx.agc.flag = TRUE;
    break;
  case agcMED:
    tx.agc.gen->AgcMode = agcMED;
    tx.agc.gen->Hang = 6;
    tx.agc.flag = TRUE;
    break;
  case agcFAST:
    tx.agc.gen->AgcMode = agcFAST;
    tx.agc.gen->Hang = 3;
    tx.agc.flag = TRUE;
    break;
  case agcLONG:
    tx.agc.gen->AgcMode = agcLONG;
    tx.agc.gen->Hang = 23;
    tx.agc.flag = TRUE;
    break;
  }
  return 0;
}

PRIVATE int
setTXAGCCompression(int n, char **p) {
  REAL txcompression = atof(p[0]);
  tx.agc.gen->MaxGain = pow(10.0 , txcompression * 0.05);
  return 0;
}

PRIVATE int
setTXAGCFF(int n, char **p) {
	int setit = atoi(p[0]);
	tx.spr.flag = setit;
	return 0;
}

PRIVATE int
setTXAGCFFCompression(int n, char **p) {
  REAL txcompression = atof(p[0]);
  tx.spr.gen->MaxGain = pow(10.0 , txcompression * 0.05);
  return 0;
}

PRIVATE int
setTXAGCHang(int n, char **p) {
  int hang = atoi(p[0]);
  tx.agc.gen->Hang =
    (int) max(1, min(23, ((REAL) hang) * uni.samplerate / (1000.0 * uni.buflen)));
  return 0;
}

PRIVATE int
setTXAGCLimit(int n, char **p) {
  REAL limit = atof(p[0]);
  tx.agc.gen->AgcLimit = 0.001 * limit;
  return 0;
}

PRIVATE int
setTXSpeechCompression(int n, char **p) {
  tx.spr.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setTXSpeechCompressionGain(int n, char **p) {
  tx.spr.gen->MaxGain = dB2lin(atof(p[0]));
  return 0;
}

//============================================================

PRIVATE int
f2x(REAL f) {
  REAL fix = tx.filt.ovsv->fftlen * f / uni.samplerate;
  return (int) (fix + 0.5);
}

//------------------------------------------------------------

PRIVATE void
apply_txeq_band(REAL lof, REAL dB, REAL hif) {
  int i,
      lox = f2x(lof),
      hix = f2x(hif),
	  l = tx.filt.ovsv->fftlen;
  REAL g = dB2lin(dB);
  COMPLEX *src = tx.filt.save,
          *trg = tx.filt.ovsv->zfvec;
   for (i = lox; i < hix; i++) {
    trg[i] = Cscl(src[i], g);
    if (i) {
      int j = l - i;
      trg[j] = Cscl(src[j], g);
    }
  }
}

// typical:
// 0 dB1 75 dB2 150 dB3 300 dB4 600 dB5 1200 dB6 2000 dB7 2800 dB8 3600
// approximates W2IHY bandcenters.
// no args, revert to no EQ.
PRIVATE int
setTXEQ(int n, char **p) {
  if (n < 3) {
    // revert to no EQ
    memcpy((char *) tx.filt.ovsv->zfvec,
	   (char *) tx.filt.save,
	   tx.filt.ovsv->fftlen * sizeof(COMPLEX));
    return 0;
  } else {
    int i;
    REAL lof = atof(p[0]);
    for (i = 0; i < n - 2; i += 2) {
      REAL dB = atof(p[i + 1]),
	   hif = atof(p[i + 2]);
      if (lof < 0.0 || hif <= lof) return -1;
      apply_txeq_band(lof, dB, hif);
      lof = hif;
    }
    return 0;
  }
}

//------------------------------------------------------------

PRIVATE void
apply_rxeq_band(REAL lof, REAL dB, REAL hif) {
  int i,
      lox = f2x(lof),
      hix = f2x(hif),
      l = rx.filt.ovsv->fftlen;
  REAL g = dB2lin(dB);
  COMPLEX *src = rx.filt.save,
          *trg = rx.filt.ovsv->zfvec;
  for (i = lox; i < hix; i++) {
    trg[i] = Cscl(src[i], g);
    if (i) {
      int j = l - i;
      trg[j] = Cscl(src[j], g);
    }
  }
}

PRIVATE int
setRXEQ(int n, char **p) {
  if (n < 3) {
    // revert to no EQ
    memcpy((char *) rx.filt.ovsv->zfvec,
	   (char *) rx.filt.save,
	   rx.filt.ovsv->fftlen * sizeof(COMPLEX));
    return 0;
  } else {
    int i;
    REAL lof = atof(p[0]);
    for (i = 0; i < n - 2; i += 2) {
      REAL dB = atof(p[i + 1]),
	   hif = atof(p[i + 2]);
      if (lof < 0.0 || hif <= lof) return -1;
      apply_rxeq_band(lof, dB, hif);
      lof = hif;
    }
    return 0;
  }
}

//============================================================
PRIVATE int
setRXAGC(int n, char **p) {
  int setit = atoi(p[0]);
  switch (setit) {
  case agcOFF:
    rx.agc.gen->AgcMode = agcOFF;
    rx.agc.flag = TRUE;
    break;
  case agcSLOW:
    rx.agc.gen->AgcMode = agcSLOW;
    rx.agc.gen->Hang = 10;
    rx.agc.flag = TRUE;
    break;
  case agcMED:
    rx.agc.gen->AgcMode = agcMED;
    rx.agc.gen->Hang = 6;
    rx.agc.flag = TRUE;
    break;
  case agcFAST:
    rx.agc.gen->AgcMode = agcFAST;
    rx.agc.gen->Hang = 3;
    rx.agc.flag = TRUE;
    break;
  case agcLONG:
    rx.agc.gen->AgcMode = agcLONG;
    rx.agc.gen->Hang = 23;
    rx.agc.flag = TRUE;
    break;
  }
  return 0;
}

PRIVATE int
setANFvals(int n, char **p) {
  int taps  = atoi(p[0]),
      delay = atoi(p[1]);
  REAL gain = atof(p[2]),
       leak = atof(p[3]);
  rx.anf.gen->adaptive_filter_size = taps;
  rx.anf.gen->delay = delay;
  rx.anf.gen->adaptation_rate = gain;
  rx.anf.gen->leakage = leak;
  return 0;
}

PRIVATE int
setNRvals(int n, char **p) {
  int taps  = atoi(p[0]),
      delay = atoi(p[1]);
  REAL gain = atof(p[2]),
       leak = atof(p[3]);
  rx.anr.gen->adaptive_filter_size = taps;
  rx.anr.gen->delay = delay;
  rx.anr.gen->adaptation_rate = gain;
  rx.anr.gen->leakage = leak;
  return 0;
}

PRIVATE int
setcorrectIQ(int n, char **p) {
  int phaseadjustment = atoi(p[0]),
      gainadjustment  = atoi(p[1]);
  rx.iqfix->phase = 0.001 * (REAL) phaseadjustment;
  rx.iqfix->gain  = 1.0+ 0.001 * (REAL) gainadjustment;
  return 0;
}

PRIVATE int
setcorrectIQphase(int n, char **p) {
  int phaseadjustment = atoi(p[0]);
  rx.iqfix->phase = 0.001 * (REAL) phaseadjustment;
  return 0;
}

PRIVATE int
setcorrectIQgain(int n, char **p) {
  int gainadjustment = atoi(p[0]);
  rx.iqfix->gain = 1.0 + 0.001 * (REAL) gainadjustment;
  return 0;
}

PRIVATE int
setSquelch(int n, char **p) {
  rx.squelch.thresh = -atof(p[0]);
  return 0;
}

PRIVATE int
setMeterOffset(int n, char **p) {
  rx.squelch.offset.meter = atof(p[0]);
  return 0;
}

PRIVATE int
setATTOffset(int n, char **p) {
  rx.squelch.offset.att = atof(p[0]);
  return 0;
}

PRIVATE int
setGainOffset(int n, char **p) {
  rx.squelch.offset.gain = atof(p[0]);
  return 0;
}

PRIVATE int
setSquelchSt(int n, char **p) {
  rx.squelch.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setTRX(int n, char **p) {
  uni.mode.trx = atoi(p[0]);
  return 0;
}

PRIVATE int
setRunState(int n, char **p) {
  RUNMODE rs = (RUNMODE) atoi(p[0]);
  top.state = rs;
  return 0;
}

PRIVATE int
setSpotToneVals(int n, char **p) {
  REAL gain  = atof(p[0]),
       freq  = atof(p[1]),
       rise  = atof(p[2]),
       fall  = atof(p[3]);
  setSpotToneGenVals(rx.spot.gen, gain, freq, rise, fall);
  return 0;
}

PRIVATE int
setSpotTone(int n, char **p) {
  if (atoi(p[0])) {
    SpotToneOn(rx.spot.gen);
    rx.spot.flag = TRUE;
  } else
    SpotToneOff(rx.spot.gen);
  return 0;
}

PRIVATE int
setRXPreScl(int n, char **p) {
  rx.scl.pre.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setRXPreSclVal(int n, char **p) {
  rx.scl.pre.val = dB2lin(atof(p[0]));
  return 0;
}

PRIVATE int
setTXPreScl(int n, char **p) {
  tx.scl.pre.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setTXPreSclVal(int n, char **p) {
  tx.scl.pre.val = dB2lin(atof(p[0]));
  return 0;
}

PRIVATE int
setRXPostScl(int n, char **p) {
  rx.scl.post.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setRXPostSclVal(int n, char **p) {
  rx.scl.post.val = dB2lin(atof(p[0]));
  return 0;
}

PRIVATE int
setTXPostScl(int n, char **p) {
  tx.scl.post.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setTXPostSclVal(int n, char **p) {
  tx.scl.post.val = dB2lin(atof(p[0]));
  return 0;
}

PRIVATE int
setFinished(int n, char **p) {
  top.running = FALSE;
  return 0;
}

PRIVATE int
setPWSmode(int n,char **p) {
    int setit = atoi(p[0]);
	uni.powerspectrum.pws->Mode = setit;
	return 0;
}

PRIVATE int
setPWSSubmode(int n,char **p) {
    int setit = atoi(p[0]);
	uni.powerspectrum.pws->SubMode = setit;
	return 0;
}

// next-trx-mode [nbufs-to-zap]
PRIVATE int
setSWCH(int n, char **p) {
  top.swch.trx.next = atoi(p[0]);
  if (n > 1) top.swch.bfct.want = atoi(p[1]);
  else top.swch.bfct.want = 0;
  top.swch.bfct.have = 0;
  top.swch.run.last = top.state;
  top.state = RUN_SWCH;
  return 0;
}

//========================================================================

#include <thunk.h>

CTE update_cmds[] = {
  {"setANF", setANF},
  {"setANFvals", setANFvals},
  {"setATTOffset", setATTOffset},
  {"setBIN", setBIN},
  {"setcorrectIQ", setcorrectIQ},
  {"setcorrectIQgain", setcorrectIQgain},
  {"setcorrectIQphase", setcorrectIQphase},
  {"setFilter", setFilter},
  {"setfixedAGC", setfixedAGC},
  {"setGainOffset", setGainOffset},
  {"setMeterOffset", setMeterOffset},
  {"setMode", setMode},
  {"setNB", setNB},
  {"setNBvals", setNBvals},
  {"setNR", setNR},
  {"setNRvals", setNRvals},
  {"setOsc", setOsc},
  {"setRXAGC", setRXAGC},
  {"setRunState", setRunState},
  {"setSampleRate", setSampleRate},
  {"setSquelch", setSquelch},
  {"setSquelchSt", setSquelchSt},
  {"setTXAGC", setTXAGC},
  {"setTXAGCCompression", setTXAGCCompression},
  {"setTXAGCFFCompression",setTXAGCFFCompression},
  {"setTXAGCHang", setTXAGCHang},
  {"setTXAGCLimit", setTXAGCLimit},
  {"setTXSpeechCompression", setTXSpeechCompression},
  {"setTXSpeechCompressionGain", setTXSpeechCompressionGain},
  {"setRXEQ", setRXEQ},
  {"setTXEQ", setTXEQ},
  {"setTRX", setTRX},
  {"setRXPreScl", setRXPreScl},
  {"setRXPreSclVal", setRXPreSclVal},
  {"setTXPreScl", setTXPreScl},
  {"setTXPreSclVal", setTXPreSclVal},
  {"setRXPostScl", setRXPostScl},
  {"setRXPostSclVal", setRXPostSclVal},
  {"setTXPostScl", setTXPostScl},
  {"setTXPostSclVal", setTXPostSclVal},
  {"setFinished", setFinished},
  {"setSWCH", setSWCH},
  {"setSpotToneVals", setSpotToneVals},
  {"setSpotTone", setSpotTone},
  {"setPWSmode", setPWSmode},
  {"setPWSSubmode",setPWSSubmode},
  {"setSDROM", setSDROM},
  {"setSDROMvals",setSDROMvals},
  { 0, 0 }
};

//........................................................................

int do_update(char *str) {
  
  SPLIT splt = &uni.update.splt;
  
  split(splt, str);
  
  if (NF(splt) < 1) return -1;
  else {
    Thunk thk = Thunk_lookup(update_cmds, F(splt, 0));
    if (!thk) {
      fprintf(stderr,"-1\n");
      return -1;
    } else {
      int val;
      WaitForSingleObject(top.sync.upd.sem,INFINITE);
      val = (*thk)(NF(splt) - 1, Fptr(splt, 1));
      ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
      return val;
    }
  }
}

BOOLEAN newupdates() {
  WaitForSingleObject(top.sync.upd2.sem,INFINITE);
  return TRUE;
}

PRIVATE void
sendcommand(char *str) {
  putChan_nowait(uni.update.chan.c,str,strlen(str)+1);;
  ReleaseSemaphore(top.sync.upd2.sem,1L,NULL);
  Sleep(0);
}

#ifdef PROCESSVERS
DttSP_EXP int
changeMode(SDRMODE mode) {
  char str[128];
  switch(mode) {
  case LSB:
    sprintf(str,"setMode %d",LSB);
    sendcommand(str);
    break;
  case USB:
    sprintf(str,"setMode %d",USB);
    sendcommand(str);
    break;
  case DSB:
    sprintf(str,"setMode %d",DSB);
    sendcommand(str);
    break;
  case CWL:
    sprintf(str,"setMode %d",CWL);
    sendcommand(str);
    break;
  case CWU:
    sprintf(str,"setMode %d",CWU);
    sendcommand(str);
    break;
  case AM:
    sprintf(str,"setMode %d",AM);
    sendcommand(str);
    break;
  case PSK:
    sprintf(str,"setMode %d",PSK);
    sendcommand(str);
    break;
  case SPEC:
    sprintf(str,"setMode %d",SPEC);
    sendcommand(str);
    break;
  case RTTY:
    sprintf(str,"setMode %d",RTTY);
    sendcommand(str);
    break;
  case SAM:
    sprintf(str,"setMode %d",SAM);
    sendcommand(str);
    break;
  case DRM:
    sprintf(str,"setMode %d",DRM);
    sendcommand(str);
    break;
  default:
    break;
  }
  return 0;
}

DttSP_EXP int
changeOsc(REAL newfreq){
  char str[64];
  if (fabs(newfreq) >= 0.5* uni.samplerate) return -1;
  sprintf(str,"setOsc %lf RX",newfreq);
  sendcommand(str);
  return 0;
}

DttSP_EXP int
changeTXOsc(REAL newfreq){
  char str[64];
  if (fabs(newfreq) >= 0.5* uni.samplerate) return -1;
  sprintf(str,"setOsc %lf TX",newfreq);
  sendcommand(str);
  return 0;
}

DttSP_EXP int
changeSampleRate(REAL newSampleRate) {
  char str[64];
  sprintf(str,"setSampleRate %9.0lf",newSampleRate);
  sendcommand(str);
  return 0;
}

DttSP_EXP void changeNR(int setit) {
  char str[64];
  sprintf(str,"setNR %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeANF(int setit) {
  char str[64];
  sprintf(str,"setANF %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeNB(int setit) {
  char str[64];
  sprintf(str,"setNB %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeNBvals(REAL threshold) {
  char str[64];
  sprintf(str,"setNBvals %lf",threshold);
  sendcommand(str);	
}


DttSP_EXP void changeBIN(int setit) {
  char str[64];
  sprintf(str,"setBIN %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeAGC(AGCMODE setit) {
  char str[64];
  switch (setit) {
  case agcOFF:
    sprintf(str,"setRXAGC %d",agcOFF);
    break;
  case agcLONG:
    sprintf(str,"setRXAGC %d",agcLONG);
    break;
  case agcSLOW:
    sprintf(str,"setRXAGC %d",agcSLOW);
    break;
  case agcMED:
    sprintf(str,"setRXAGC %d",agcMED);
    break;
  case agcFAST:
    sprintf(str,"setRXAGC %d",agcFAST);
    break;
  }
  sendcommand(str);
}

DttSP_EXP void changeANFvals(int taps, int delay, REAL gain, REAL leak) {
  char str[128];
  sprintf(str,"setANFvals %3d %3d %9.7lf %9.7lf",taps,delay,gain,leak);
  sendcommand(str);
}

DttSP_EXP void changeNRvals(int taps, int delay, REAL gain, REAL leak) {
  char str[128];
  sprintf(str,"setNRvals %3d %3d %9.7lf %9.7lf",taps,delay,gain,leak);
  sendcommand(str);
}

DttSP_EXP void setcorrectIQcmd(int phaseadjustment,int gainadjustment) {
  char str[64];
  sprintf(str,"setcorrectIQ %d %d",phaseadjustment,gainadjustment);
  sendcommand(str);
}

DttSP_EXP void changecorrectIQphase(int phaseadjustment){
  char str[64];
  sprintf(str,"setcorrectIQphase %d",phaseadjustment);
  sendcommand(str);
}

DttSP_EXP void changecorrectIQgain(int gainadjustment){
  char str[64];
  sprintf(str,"setcorrectIQgain %d",gainadjustment);
  sendcommand(str);
}

DttSP_EXP int
changeFilter(REAL low_frequency,REAL high_frequency,int ncoef, TRXMODE trx) {
  char str[64];
  if (trx == RX)  sprintf(str,"setFilter %6.0lf %6.0lf RX",low_frequency,high_frequency);
  else sprintf(str,"setFilter %6.0lf %6.0lf TX",low_frequency,high_frequency);
  sendcommand(str);
  return 0;
}

DttSP_EXP void
changePWSmode(PWSMODE setit) {
  char str[64];
  switch (setit) {
  case PREFILTER:
    sprintf(str,"setPWSmode %d",PREFILTER);
    break;
  case POSTFILTER:
    sprintf(str,"setPWSmode %d",POSTFILTER);
    break;
  case AUDIO:
    sprintf(str,"setPWSmode %d",AUDIO);
    break;
  default:
    return;
  }
  sendcommand(str);
}

DttSP_EXP void
changePWSsubmode(PWSSUBMODE setit) {
  char str[64];
  switch (setit) {
  case SPECTRUM:
    sprintf(str,"setPWSSubmode %d",SPECTRUM);
    break;
  case PHASE:
    sprintf(str,"setPWSSubmode %d",PHASE);
    break;
  case SCOPE:
    sprintf(str,"setPWSSubmode %d",SCOPE);
    break;
  case PHASE2:
    sprintf(str,"setPWSSubmode %d",PHASE);
    break;
  case WATERFALL:
    sprintf(str,"setPWSSubmode %d",WATERFALL);
    break;
  case HISTOGRAM:
    sprintf(str,"setPWSSubmode %d",HISTOGRAM);
    break;
  default:
    return;
  }
  sendcommand(str);
}

DttSP_EXP void
changeWindow(Windowtype Windowset){
}

DttSP_EXP void
oldsetTXEQ(int *txeq) {
  char str[256];
  sprintf(str,
	  "setTXEQ 0 %d 450 %d 800 %d 1150 %d 1450 %d 1800 %d 2150 %d 2450 %d 2800 %d 3600",
	  txeq[0],
	  txeq[1],
	  txeq[2],
	  txeq[3],
	  txeq[4],
	  txeq[5],
	  txeq[6],
	  txeq[7],
	  txeq[8]);
  sendcommand(str);
}

DttSP_EXP void
changeTXAGCCompression(double txc) {
  char str[64];
  sprintf(str,"setTXAGCCompression %lf",txc);
  sendcommand(str);
}

DttSP_EXP void
changeTXAGCFFCompression(double txc) {
  char str[64];
  sprintf(str,"setTXAGCFFCompression %lf",txc);
  sendcommand(str);
}

DttSP_EXP void oldsetGainOffset(float val) {
  char str[64];
  sprintf(str,"setGainOffset %f",val);
  sendcommand(str);
}

DttSP_EXP void setTXScale(REAL scale) {
  char str[32];
  sprintf(str,"setTXPreScl 1 ");
  sendcommand(str);
  Sleep(0);
  sprintf(str,"setTXPreSclVal %lf",20.0*log10(scale));
  sendcommand(str);
}

DttSP_EXP void oldsetATTOffset(float val){
  char str[64];
  sprintf(str,"setATTOffset %f",val);
  sendcommand(str);
}

DttSP_EXP setRXScale(float val) {
  char str[64];
  sprintf(str,"setRXPostScl 1");
  sendcommand(str);
  Sleep(0);
  sprintf(str,"setRXPostSclVal %f",val);
  sendcommand(str);
}

DttSP_EXP oldsetMeterOffset(float val){
  char str[64];
  sprintf(str,"setMeterOffset %f",val);
  sendcommand(str);
}

DttSP_EXP changeSquelch(float setit) {
  char str[64];
  sprintf(str,"setSquelch %f",setit);
  sendcommand(str);
}

DttSP_EXP changeSquelchSt(BOOLEAN setit) {
  char str[64];
  sprintf(str,"setSquelch %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeSDROM(BOOLEAN setit) {   
  char str[64];
  sprintf(str,"setSDROM %d",setit);
  sendcommand(str);
}

DttSP_EXP void changeSDROMvals(REAL threshold) {
  char str[64];
  sprintf(str,"setSDROMvals %lf",threshold);
  sendcommand(str);
}


#endif   // END PROCESSVERS



#ifdef DLLVERS
DttSP_EXP int
changeMode(SDRMODE mode) {
  switch(mode) {
  case LSB:
  case USB:
  case DSB:
  case CWL:
  case CWU:
  case FMN:
  case AM:
  case PSK:
  case SPEC:
  case RTTY:
  case SAM:
  case DRM:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.mode = tx.mode = uni.mode.sdr = mode;
    if (mode == SAM) rx.am.gen->mode = SAMdet;
    if (mode == AM)  rx.am.gen->mode = AMdet;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  default:
    break;
  }
  return 0;
}

DttSP_EXP int
changeOsc(REAL newfreq){
  if (fabs(newfreq) >= 0.5* uni.samplerate) return -1;
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.osc.gen->Frequency = rx.osc.freq = 2.0*M_PI*newfreq/uni.samplerate;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
  return 0;
}

DttSP_EXP int
changeTXOsc(REAL newfreq){
  if (fabs(newfreq) >= 0.5* uni.samplerate) return -1;
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.osc.gen->Frequency = tx.osc.freq = 2.0*M_PI*newfreq/uni.samplerate;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
  return 0;
}

DttSP_EXP int
changeSampleRate(REAL newSampleRate) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  uni.samplerate = newSampleRate;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
  return 0;
}

DttSP_EXP void changeNR(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.anr.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeANF(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.anf.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeNB(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.nb.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeNBvals(REAL threshold) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.nb.gen->threshold = rx.nb.thresh = threshold;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeSDROM(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.nb_sdrom.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeSDROMvals(REAL threshold) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.nb_sdrom.gen->threshold = rx.nb_sdrom.thresh = threshold;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}


DttSP_EXP void changeBIN(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.bin.flag= setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeAGC(AGCMODE setit) {
  switch (setit) {
  case agcOFF:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.agc.gen->AgcMode = setit;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  case agcLONG:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.agc.gen->AgcMode = setit;
    rx.agc.gen->Hang = 23;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  case agcSLOW:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.agc.gen->AgcMode = setit;
    rx.agc.gen->Hang = 7;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  case agcMED:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.agc.gen->AgcMode = setit;
    rx.agc.gen->Hang = 4;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  case agcFAST:
    WaitForSingleObject(top.sync.upd.sem,INFINITE);
    rx.agc.gen->AgcMode = setit;
    rx.agc.gen->Hang = 2;
    ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
    break;
  default:
    break;
  }
}

DttSP_EXP void changefixedAGC(REAL fixed_agc) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.agc.gen->AgcFixedGain = dB2lin(fixed_agc);
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changemaxAGC(REAL max_agc) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.agc.gen->MaxGain = dB2lin(max_agc);
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeRXAGCAttackTimeScale(int limit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.agc.gen->AgcAttackTimeScale = limit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);

}

DttSP_EXP void changeRXAGCHang(int limit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.agc.gen->Hang = limit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);

}

DttSP_EXP void changeRXAGCLimit(int limit){
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.agc.gen->AgcLimit = 0.001 * (double)limit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeANFvals(int taps, int delay, REAL gain, REAL leak) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.anf.gen->adaptation_rate = gain;
  rx.anf.gen->adaptive_filter_size = taps;
  rx.anf.gen->delay = delay;
  rx.anf.gen->leakage = leak;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void changeNRvals(int taps, int delay, REAL gain, REAL leak) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.anr.gen->adaptation_rate = gain;
  rx.anr.gen->adaptive_filter_size = taps;
  rx.anr.gen->delay = delay;
  rx.anr.gen->leakage = leak;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
setcorrectIQcmd(int phaseadjustment,int gainadjustment) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.iqfix->phase = 0.001 * (REAL) phaseadjustment; 
  rx.iqfix->gain  = 1.0 + 0.001 * (REAL) gainadjustment;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changecorrectIQphase(int phaseadjustment) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.iqfix->phase = 0.001 * (REAL) phaseadjustment; 
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changecorrectIQgain(int gainadjustment) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.iqfix->gaina = 1.0 + 0.001 * (REAL) gainadjustment;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP int
changeFilter(REAL low_frequency,REAL high_frequency,int ncoef, TRXMODE trx) {
  int i, fftlen = 2 * uni.buflen;
  fftw_plan ptmp;
  COMPLEX *zcvec;
  ncoef = uni.buflen + 1;

  if (fabs(low_frequency) >= 0.5 * uni.samplerate) return -1;
  if (fabs(high_frequency) >= 0.5 * uni.samplerate) return -2;
  if ((low_frequency + 10) >= high_frequency) return -3;
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  if (trx == RX) {
    delFIR_COMPLEX(rx.filt.coef);

    rx.filt.coef = newFIR_Bandpass_COMPLEX(low_frequency,
					   high_frequency,
					   uni.samplerate,
					   ncoef + 1);

    zcvec = newvec_COMPLEX(fftlen, "filter z vec in setFilter");
    ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, FFTW_OUT_OF_PLACE);
#ifdef LHS
    for (i = 0; i < ncoef; i++) zcvec[i] = rx.filt.coef->coef[i];
#else
    for (i = 0; i < ncoef; i++) zcvec[fftlen - ncoef + i] = rx.filt.coef->coef[i];
#endif
    fftw_one(ptmp, (fftw_complex *) zcvec, (fftw_complex *) rx.filt.ovsv->zfvec);
    fftw_destroy_plan(ptmp);
    delvec_COMPLEX(zcvec);
    memcpy((char *) rx.filt.save,
	   (char *) rx.filt.ovsv->zfvec,
	   rx.filt.ovsv->fftlen * sizeof(COMPLEX));
  } else {
    delFIR_COMPLEX(tx.filt.coef);

    tx.filt.coef = newFIR_Bandpass_COMPLEX(low_frequency,
					   high_frequency,
					   uni.samplerate,
					   ncoef + 1);

    zcvec = newvec_COMPLEX(fftlen, "filter z vec in setFilter");
    ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, FFTW_OUT_OF_PLACE);
#ifdef LHS
    for (i = 0; i < ncoef; i++) zcvec[i] = tx.filt.coef->coef[i];
#else
    for (i = 0; i < ncoef; i++) zcvec[fftlen - ncoef + i] = tx.filt.coef->coef[i];
#endif
    fftw_one(ptmp, (fftw_complex *) zcvec, (fftw_complex *) tx.filt.ovsv->zfvec);
    fftw_destroy_plan(ptmp);
    delvec_COMPLEX(zcvec);
    memcpy((char *) tx.filt.save,
	   (char *) tx.filt.ovsv->zfvec,
	   tx.filt.ovsv->fftlen * sizeof(COMPLEX));

  }
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
  return 0;
}

DttSP_EXP void
changePWSmode(PWSMODE setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  uni.powerspectrum.pws->Mode = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);

}

DttSP_EXP void
changePWSsubmode(PWSSUBMODE setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  uni.powerspectrum.pws->SubMode = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeWindow(Windowtype Windowset){
}

DttSP_EXP void
oldsetTXEQ(int *txeq) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  apply_txeq_band(0.0   ,txeq[ 0],120.0);
  apply_txeq_band(120.0 ,txeq[ 1],230.0);
  apply_txeq_band(230.0 ,txeq[ 2],450.0);
  apply_txeq_band(450.0 ,txeq[ 3],800.0);
  apply_txeq_band(800.0 ,txeq[ 4],1150.0);
  apply_txeq_band(1150.0,txeq[ 5],1450.0);
  apply_txeq_band(1450.0,txeq[ 6],1800.0);
  apply_txeq_band(1800.0,txeq[ 7],2150.0);
  apply_txeq_band(2150.0,txeq[ 8],2450.0);
  apply_txeq_band(2450.0,txeq[ 9],2800.0);
  apply_txeq_band(2800.0,txeq[10],3250.0);
  apply_txeq_band(3250.0,txeq[11],6000.0);
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeTXAGCCompression(double txc) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.agc.gen->MaxGain = pow(10.0 , txc * 0.05);
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeTXAGCFF(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.spr.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeTXAGCLimit(int setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.agc.gen->AgcLimit = 0.001*(REAL)setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeTXAGCHang(int hang) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.agc.gen->Hang = (int)max(1,min(23,((REAL)hang) * uni.samplerate /(1000.0*uni.buflen)));
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void
changeTXAGCFFCompression(REAL txc) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.spr.gen->MaxGain = pow(10.0 , txc * 0.05);
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void setTXScale(REAL scale) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  tx.scl.post.flag = TRUE;
  tx.scl.post.val = scale;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void oldsetGainOffset(float val) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.squelch.offset.gain = (REAL)val;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP void oldsetATTOffset(float val){
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.squelch.offset.att = (REAL)val;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP oldsetMeterOffset(float val){
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.squelch.offset.meter = (REAL)val;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}


DttSP_EXP setRXScale(REAL val) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.scl.post.flag = TRUE;
  rx.scl.post.val = val;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP changeSquelch(int setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.squelch.thresh = -(REAL)setit;	
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}

DttSP_EXP changeSquelchSt(BOOLEAN setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  rx.squelch.flag = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);

}

DttSP_EXP void oldsetTRX(TRXMODE setit) {
  WaitForSingleObject(top.sync.upd.sem,INFINITE);
  /*	top.swch.trx.next = setit;
	top.swch.bfct.want = 2;
	top.swch.bfct.have = 0;
	top.swch.run.last = top.state;
	top.state = RUN_SWCH; */
  uni.mode.trx = setit;
  ReleaseSemaphore(top.sync.upd.sem,1L,NULL);
}
#endif    //  END DLLVERS



