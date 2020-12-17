/* update.c

common defs and code for parm update 
   
This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2004-5 by Frank Brickle, AB2KT and Bob McGwier, N4HY

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
// for commands affecting RX, which RX is Listening

#define RL (uni.multirx.lis)

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
  delFIR_COMPLEX(rx[RL].filt.coef);

  rx[RL].filt.coef = newFIR_Bandpass_COMPLEX(low_frequency,
					     high_frequency,
					     uni.samplerate,
					     ncoef);

  zcvec = newvec_COMPLEX(fftlen, "filter z vec in setFilter");
  ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, uni.wisdom.bits);
#ifdef LHS
  for (i = 0; i < ncoef; i++)
    zcvec[i] = rx[RL].filt.coef->coef[i];
#else
  for (i = 0; i < ncoef; i++)
    zcvec[fftlen - ncoef + i] = rx[RL].filt.coef->coef[i];
#endif
  fftw_one(ptmp,
	   (fftw_complex *) zcvec,
	   (fftw_complex *) rx[RL].filt.ovsv->zfvec);
  fftw_destroy_plan(ptmp);
  delvec_COMPLEX(zcvec);
  normalize_vec_COMPLEX(rx[RL].filt.ovsv->zfvec,
			rx[RL].filt.ovsv->fftlen);
  memcpy((char *) rx[RL].filt.save,
	 (char *) rx[RL].filt.ovsv->zfvec,
	 rx[RL].filt.ovsv->fftlen * sizeof(COMPLEX));

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
  ptmp = fftw_create_plan(fftlen, FFTW_FORWARD, uni.wisdom.bits);
#ifdef LHS
  for (i = 0; i < ncoef; i++)
    zcvec[i] = tx.filt.coef->coef[i];
#else
  for (i = 0; i < ncoef; i++)
    zcvec[fftlen - ncoef + i] = tx.filt.coef->coef[i];
#endif
  fftw_one(ptmp,
	   (fftw_complex *) zcvec,
	   (fftw_complex *) tx.filt.ovsv->zfvec);
  fftw_destroy_plan(ptmp);
  delvec_COMPLEX(zcvec);
  normalize_vec_COMPLEX(tx.filt.ovsv->zfvec,
			tx.filt.ovsv->fftlen);
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
    default: rx[RL].mode = mode; break;
    }
  } else
    tx.mode = rx[RL].mode = uni.mode.sdr = mode;
  if (rx[RL].mode == AM) rx[RL].am.gen->mode = AMdet;
  if (rx[RL].mode == SAM) rx[RL].am.gen->mode = SAMdet;
  return 0;
}

// setOsc <freq> [TRX]
PRIVATE int
setOsc(int n, char **p) {
  REAL newfreq = atof(p[0]);
  if (fabs(newfreq) >= 0.5 * uni.samplerate) return -1;
  newfreq *= 2.0 * M_PI / uni.samplerate;
  if (n > 1) {
    int trx = atoi(p[1]);
    switch (trx) {
    case TX: tx.osc.gen->Frequency = newfreq; break;
    case RX:
    default: rx[RL].osc.gen->Frequency = newfreq; break;
    }
  } else
    tx.osc.gen->Frequency = rx[RL].osc.gen->Frequency = newfreq;
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
  rx[RL].anr.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setANF(int n, char **p) {
  rx[RL].anf.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setNB(int n, char **p) {
  rx[RL].nb.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setNBvals(int n, char **p) {
  REAL threshold = atof(p[0]);
  rx[RL].nb.gen->threshold = rx[RL].nb.thresh = threshold;
  return 0;
}

PRIVATE int
setSDROM(int n, char **p) {
  rx[RL].nb_sdrom.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setSDROMvals(int n, char **p) {
 REAL threshold = atof(p[0]);
  rx[RL].nb_sdrom.gen->threshold = rx[RL].nb_sdrom.thresh = threshold;
  return 0;
}

PRIVATE int
setBIN(int n, char **p) {
  rx[RL].bin.flag = atoi(p[0]);
  return 0;
}

// setfixedAGC <gain> [TRX]
PRIVATE int
setfixedAGC(int n, char **p) {
  REAL gain = atof(p[0]);
  if (n > 1) {
    int trx = atoi(p[1]);
    switch(trx) {
    case TX: tx.agc.gen->gain.now = gain; break;
    case RX:
    default: rx[RL].agc.gen->gain.now = gain; break;
    }
  } else
    tx.agc.gen->gain.now = rx[RL].agc.gen->gain.now = gain;
  return 0;
}

PRIVATE int
setRXAGC(int n, char **p) {
  int setit = atoi(p[0]);
  switch (setit) {
  case agcOFF:
    rx[RL].agc.gen->mode = agcOFF;
    rx[RL].agc.flag = TRUE;
    break;
  case agcSLOW:
    rx[RL].agc.gen->mode = agcSLOW;
    rx[RL].agc.gen->hang = 10;
    rx[RL].agc.flag = TRUE;
    break;
  case agcMED:
    rx[RL].agc.gen->mode = agcMED;
    rx[RL].agc.gen->hang = 6;
    rx[RL].agc.flag = TRUE;
    break;
  case agcFAST:
    rx[RL].agc.gen->mode = agcFAST;
    rx[RL].agc.gen->hang = 3;
    rx[RL].agc.flag = TRUE;
    break;
  case agcLONG:
    rx[RL].agc.gen->mode = agcLONG;
    rx[RL].agc.gen->hang = 23;
    rx[RL].agc.flag = TRUE;
    break;
  }
  return 0;
}

PRIVATE int
setRXAGCCompression(int n, char **p) {
  REAL rxcompression = atof(p[0]);
  rx[RL].agc.gen->gain.top = pow(10.0 , rxcompression * 0.05);
  return 0;
}

PRIVATE int
setRXAGCHang(int n, char **p) {
  int hang = atoi(p[0]);
  rx[RL].agc.gen->hang =
    max(1,
	min(23,
	    hang * uni.samplerate / (1e3 * uni.buflen)));
  return 0;
}

PRIVATE int
setRXAGCLimit(int n, char **p) {
  REAL limit = atof(p[0]);
  rx[RL].agc.gen->gain.lim = 0.001 * limit;
  return 0;
}

PRIVATE int
setTXAGC(int n, char **p) {
  int setit = atoi(p[0]);
  switch (setit) {
  case agcOFF:
    tx.agc.gen->mode = agcOFF;
    tx.agc.flag = FALSE;
    break;
  case agcSLOW:
    tx.agc.gen->mode = agcSLOW;
    tx.agc.gen->hang = 10;
    tx.agc.flag = TRUE;
    break;
  case agcMED:
    tx.agc.gen->mode = agcMED;
    tx.agc.gen->hang = 6;
    tx.agc.flag = TRUE;
    break;
  case agcFAST:
    tx.agc.gen->mode = agcFAST;
    tx.agc.gen->hang = 3;
    tx.agc.flag = TRUE;
    break;
  case agcLONG:
    tx.agc.gen->mode = agcLONG;
    tx.agc.gen->hang = 23;
    tx.agc.flag = TRUE;
    break;
  }
  return 0;
}

PRIVATE int
setTXAGCCompression(int n, char **p) {
  REAL txcompression = atof(p[0]);
  tx.agc.gen->gain.top = pow(10.0 , txcompression * 0.05);
  return 0;
}

PRIVATE int
setTXAGCFF(int n, char **p) {
  tx.spr.flag = atoi(p[0]);
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
  tx.agc.gen->hang =
    max(1,
	min(23,
	    hang * uni.samplerate / (1e3 * uni.buflen)));
  return 0;
}

PRIVATE int
setTXAGCLimit(int n, char **p) {
  REAL limit = atof(p[0]);
  tx.agc.gen->gain.lim = 0.001 * limit;
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
// some changes have been made to a transfer function in vec;
// apply time-domain window to counter possible artifacts

PRIVATE void
re_window(COMPLEX *vec, int len) {
  int i;
  REAL *win = newvec_REAL(len, "re_window win vec");
  COMPLEX *ztmp = newvec_COMPLEX(len, "re_window z buf");

  fftw_plan ptmp = fftw_create_plan(len, FFTW_BACKWARD, uni.wisdom.bits);
  fftw_one(ptmp, (fftw_complex *) vec, (fftw_complex *) ztmp);
  fftw_destroy_plan(ptmp);

  (void) makewindow(BLACKMANHARRIS_WINDOW, len, win);

  for (i = 0; i < len; i++)
    ztmp[i] = Cscl(ztmp[i], win[i]);

  ptmp = fftw_create_plan(len, FFTW_FORWARD, uni.wisdom.bits);
  fftw_one(ptmp, (fftw_complex *) ztmp, (fftw_complex *) vec);
  fftw_destroy_plan(ptmp);

  delvec_COMPLEX(ztmp);
  delvec_REAL(win);
  
  normalize_vec_COMPLEX(vec, len);
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
// NB these are shelves, not linear or other splines

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
    re_window(rx[RL].filt.ovsv->zfvec, rx[RL].filt.ovsv->fftlen);
    return 0;
  }
}

//------------------------------------------------------------

PRIVATE void
apply_rxeq_band(REAL lof, REAL dB, REAL hif) {
  int i,
      lox = f2x(lof),
      hix = f2x(hif),
      l = rx[RL].filt.ovsv->fftlen;
  REAL g = dB2lin(dB);
  COMPLEX *src = rx[RL].filt.save,
          *trg = rx[RL].filt.ovsv->zfvec;
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
    memcpy((char *) rx[RL].filt.ovsv->zfvec,
	   (char *) rx[RL].filt.save,
	   rx[RL].filt.ovsv->fftlen * sizeof(COMPLEX));
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
    re_window(rx[RL].filt.ovsv->zfvec, rx[RL].filt.ovsv->fftlen);
    return 0;
  }
}

//============================================================

PRIVATE int
setANFvals(int n, char **p) {
  int taps  = atoi(p[0]),
      delay = atoi(p[1]);
  REAL gain = atof(p[2]),
       leak = atof(p[3]);
  rx[RL].anf.gen->adaptive_filter_size = taps;
  rx[RL].anf.gen->delay = delay;
  rx[RL].anf.gen->adaptation_rate = gain;
  rx[RL].anf.gen->leakage = leak;
  return 0;
}

PRIVATE int
setNRvals(int n, char **p) {
  int taps  = atoi(p[0]),
      delay = atoi(p[1]);
  REAL gain = atof(p[2]),
       leak = atof(p[3]);
  rx[RL].anr.gen->adaptive_filter_size = taps;
  rx[RL].anr.gen->delay = delay;
  rx[RL].anr.gen->adaptation_rate = gain;
  rx[RL].anr.gen->leakage = leak;
  return 0;
}

PRIVATE int
setcorrectIQ(int n, char **p) {
  int phaseadjustment = atoi(p[0]),
      gainadjustment  = atoi(p[1]);
  rx[RL].iqfix->phase = 0.001 * (REAL) phaseadjustment;
  rx[RL].iqfix->gain  = 1.0+ 0.001 * (REAL) gainadjustment;
  return 0;
}

PRIVATE int
setcorrectIQphase(int n, char **p) {
  int phaseadjustment = atoi(p[0]);
  rx[RL].iqfix->phase = 0.001 * (REAL) phaseadjustment;
  return 0;
}

PRIVATE int
setcorrectIQgain(int n, char **p) {
  int gainadjustment = atoi(p[0]);
  rx[RL].iqfix->gain = 1.0 + 0.001 * (REAL) gainadjustment;
  return 0;
}

PRIVATE int
setSquelch(int n, char **p) {
  rx[RL].squelch.thresh = atof(p[0]);
  return 0;
}

PRIVATE int
setSquelchSt(int n, char **p) {
  rx[RL].squelch.flag = atoi(p[0]);
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
  REAL gain = atof(p[0]),
       freq = atof(p[1]),
       rise = atof(p[2]),
       fall = atof(p[3]);
  setSpotToneGenVals(rx[RL].spot.gen, gain, freq, rise, fall);
  return 0;
}

PRIVATE int
setSpotTone(int n, char **p) {
  if (atoi(p[0])) {
    SpotToneOn(rx[RL].spot.gen);
    rx[RL].spot.flag = TRUE;
  } else
    SpotToneOff(rx[RL].spot.gen);
  return 0;
}

PRIVATE int
setRXPreScl(int n, char **p) {
  rx[RL].scl.pre.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setRXPreSclVal(int n, char **p) {
  rx[RL].scl.pre.val = dB2lin(atof(p[0]));
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
  rx[RL].scl.post.flag = atoi(p[0]);
  return 0;
}

PRIVATE int
setRXPostSclVal(int n, char **p) {
  rx[RL].scl.post.val = dB2lin(atof(p[0]));
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
  pthread_cancel(top.thrd.trx.id);
  pthread_cancel(top.thrd.mon.id);
  pthread_cancel(top.thrd.pws.id);
  pthread_cancel(top.thrd.mtr.id);
  pthread_cancel(top.thrd.upd.id);
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

PRIVATE int
setMonDump(int n, char **p) {
  sem_post(&top.sync.mon.sem);
  return 0;
}

PRIVATE int
setRingBufferReset(int n, char **p) {
  extern void clear_jack_ringbuffer(jack_ringbuffer_t *rb, int nbytes);
  jack_ringbuffer_reset(top.jack.ring.i.l);
  jack_ringbuffer_reset(top.jack.ring.i.r);
  jack_ringbuffer_reset(top.jack.ring.o.l);
  jack_ringbuffer_reset(top.jack.ring.o.r);
  clear_jack_ringbuffer(top.jack.ring.o.l, top.hold.size.bytes);
  clear_jack_ringbuffer(top.jack.ring.o.r, top.hold.size.bytes);
  return 0;
}

PRIVATE int
setRXListen(int n, char **p) {
  int lis = atoi(p[0]);
  if (lis < 0 || lis >= uni.multirx.nrx)
    return -1;
  else {
    uni.multirx.lis = lis;
    return 0;
  }
}

PRIVATE int
setRXOn(int n, char **p) {
  if (n < 1) {
    if (uni.multirx.act[RL])
      return -1;
    else {
      uni.multirx.act[RL] = TRUE;
      uni.multirx.nac++;
      rx[RL].tick = 0;
      return 0;
    }
  } else {
    int k = atoi(p[0]);
    if (k < 0 || k >= uni.multirx.nrx)
      return -1;
    else {
      if (uni.multirx.act[k])
	return -1;
      else {
	uni.multirx.act[k] = TRUE;
	uni.multirx.nac++;
	rx[k].tick = 0;
	return 0;
      }
    }
  }
}

PRIVATE int
setRXOff(int n, char **p) {
  if (n < 1) {
    if (!uni.multirx.act[RL])
      return -1;
    else {
      uni.multirx.act[RL] = FALSE;
      --uni.multirx.nac;
      return 0;
    }
  } else {
    int k = atoi(p[0]);
    if (k < 0 || k >= uni.multirx.nrx)
      return -1;
    else {
      if (!uni.multirx.act[k])
	return -1;
      else {
	uni.multirx.act[k] = FALSE;
	--uni.multirx.nac;
	return 0;
      }
    }
  }
}

// [pos]  0.0 <= pos <= 1.0
PRIVATE int
setRXPan(int n, char **p) {
  REAL pos, theta;
  if (n < 1) {
    pos = 0.5;
    theta = (1.0 - pos) * M_PI / 2.0;
    rx[RL].azim = Cmplx(cos(theta), sin(theta));
    return 0;
  } else {
    if ((pos = atof(p[0])) < 0.0 || pos > 1.0)
      return -1;
    theta = (1.0 - pos) * M_PI / 2.0;
    rx[RL].azim = Cmplx(cos(theta), sin(theta));
    return 0;
  }
}

PRIVATE int
setAuxMixSt(int n, char **p) {
  if (n < 1) {
    uni.mix.rx.flag = uni.mix.tx.flag = FALSE;
    return 0;
  } else {
    BOOLEAN flag = atoi(p[0]);
    if (n > 1) {
      switch (atoi(p[1])) {
      case TX: uni.mix.tx.flag = flag; break;
      case RX:
      default: uni.mix.rx.flag = flag; break;
      }
    } else
      uni.mix.rx.flag = uni.mix.tx.flag = flag;
    return 0;
  }
}

// [dB] NB both channels
PRIVATE int
setAuxMixGain(int n, char **p) {
  if (n < 1) {
    uni.mix.rx.gain = uni.mix.tx.gain = 1.0;
    return 0;
  } else {
    REAL gain = dB2lin(atof(p[0]));
    if (n > 1) {
      switch (atoi(p[1])) {
      case TX: uni.mix.tx.gain = gain; break;
      case RX:
      default: uni.mix.rx.gain = gain; break;
      }
    } else
      uni.mix.rx.gain = uni.mix.tx.gain = gain;
    return 0;
  }
}

PRIVATE int
setCompandSt(int n, char **p) {
  if (n < 1) {
    tx.cpd.flag = FALSE;
    return 0;
  } else {
    BOOLEAN flag = atoi(p[0]);
    if (n > 1) {
      switch (atoi(p[1])) {
      case RX: rx[RL].cpd.flag = flag; break;
      case TX:
      default: tx.cpd.flag = flag; break;
      }
    } else
      tx.cpd.flag = flag;
    return 0;
  }
}

PRIVATE int
setCompand(int n, char **p) {
  if (n < 1)
    return -1;
  else {
    REAL fac = atof(p[0]);
    if (n > 1) {
      switch (atoi(p[1])) {
      case RX: WSCReset(rx[RL].cpd.gen, fac); break;
      case TX:
      default: WSCReset(tx.cpd.gen, fac); break;
      }
    } else
      WSCReset(tx.cpd.gen, fac);
    return 0;
  }
}

//------------------------------------------------------------------------

// [type]
PRIVATE int
setMeterType(int n, char **p) {
  if (n < 1)
    uni.meter.rx.type = SIGNAL_STRENGTH;
  else {
    METERTYPE type = (METERTYPE) atoi(p[0]);
    if (n > 1) {
      int trx = atoi(p[1]);
      switch (trx) {
      case TX: uni.meter.tx.type = type; break;
      case RX:
      default: uni.meter.rx.type = type; break;
      }
    } else
       uni.meter.rx.type = type;
  }
  return 0;
}

// setSpectrumType [type [scale [rx]]]
PRIVATE int
setSpectrumType(int n, char **p) {
  uni.spec.type  = SPEC_POST_FILT;
  uni.spec.scale = SPEC_PWR;
  uni.spec.rxk   = RL;
  switch (n) {
  case 3:
    uni.spec.rxk = atoi(p[2]);
  case 2:
    uni.spec.scale = atoi(p[1]);
    break;
  case 1:
    uni.spec.type = atoi(p[0]);
    break;
  case 0:
    break;
  default:
    return -1;
  }
  return uni.spec.type;
}

PRIVATE int
setDCBlockSt(int n, char **p) {
  if (n < 1) {
    tx.dcb.flag = FALSE;
    return 0;
  } else {
    tx.dcb.flag = atoi(p[0]);
    return 0;
  }
}

PRIVATE int
setDCBlock(int n, char **p) {
  if (n < 1)
    return -1;
  else {
    resetDCBlocker(tx.dcb.gen, atoi(p[0]));
    return 0;
  }
}

//========================================================================

// save current state while guarded by upd sem
PRIVATE int
reqMeter(int n, char **p) {
  snap_meter(&uni.meter, n > 0 ? atoi(p[0]) : 0);
  sem_post(&top.sync.mtr.sem);
  return 0;
}

// simile modo
PRIVATE int
reqSpectrum(int n, char **p) {
  snap_spectrum(&uni.spec, n > 0 ? atoi(p[0]) : 0);
  sem_post(&top.sync.pws.sem);
  return 0;
}

//========================================================================

#include <thunk.h>

CTE update_cmds[] = {
  {"reqMeter", reqMeter},
  {"reqSpectrum", reqSpectrum},
  {"setANF", setANF},
  {"setANFvals", setANFvals},
  {"setBIN", setBIN},
  {"setFilter", setFilter},
  {"setFinished", setFinished},
  {"setMode", setMode},
  {"setNB", setNB},
  {"setNBvals", setNBvals},
  {"setNR", setNR},
  {"setNRvals", setNRvals},
  {"setOsc", setOsc},
  {"setRXAGC", setRXAGC},
  {"setRXAGCCompression", setRXAGCCompression},
  {"setRXAGCHang", setRXAGCHang},
  {"setRXAGCLimit", setRXAGCLimit},
  {"setRXEQ", setRXEQ},
  {"setRXPostScl", setRXPostScl},
  {"setRXPostSclVal", setRXPostSclVal},
  {"setRXPreScl", setRXPreScl},
  {"setRXPreSclVal", setRXPreSclVal},
  {"setRunState", setRunState},
  {"setSDROM", setSDROM},
  {"setSDROMvals",setSDROMvals},
  {"setSWCH", setSWCH},
  {"setSampleRate", setSampleRate},
  {"setSpotTone", setSpotTone},
  {"setSpotToneVals", setSpotToneVals},
  {"setSquelch", setSquelch},
  {"setSquelchSt", setSquelchSt},
  {"setTRX", setTRX},
  {"setTXAGC", setTXAGC},
  {"setTXAGCCompression", setTXAGCCompression},
  {"setTXAGCFFCompression",setTXAGCFFCompression},
  {"setTXAGCHang", setTXAGCHang},
  {"setTXAGCLimit", setTXAGCLimit},
  {"setTXEQ", setTXEQ},
  {"setTXPostScl", setTXPostScl},
  {"setTXPostSclVal", setTXPostSclVal},
  {"setTXPreScl", setTXPreScl},
  {"setTXPreSclVal", setTXPreSclVal},
  {"setTXSpeechCompression", setTXSpeechCompression},
  {"setTXSpeechCompressionGain", setTXSpeechCompressionGain},
  {"setcorrectIQ", setcorrectIQ},
  {"setcorrectIQgain", setcorrectIQgain},
  {"setcorrectIQphase", setcorrectIQphase},
  {"setfixedAGC", setfixedAGC},
  {"setMonDump", setMonDump},
  {"setRingBufferReset", setRingBufferReset},
  {"setRXListen", setRXListen},
  {"setRXOn", setRXOn},
  {"setRXOff", setRXOff},
  {"setRXPan", setRXPan},
  {"setAuxMixSt", setAuxMixSt},
  {"setAuxMixGain", setAuxMixGain},
  {"setCompandSt", setCompandSt},
  {"setCompand", setCompand},
  {"setMeterType", setMeterType},
  {"setSpectrumType", setSpectrumType},
  {"setDCBlockSt", setDCBlockSt},
  {"setDCBlock", setDCBlock},
  { 0, 0 }
};

//........................................................................

int
do_update(char *str, FILE *log) {
  BOOLEAN quiet = FALSE;
  SPLIT splt = &uni.update.splt;

  if (*str == '-') {
    quiet = TRUE;
    str++;
  }

  split(splt, str);

  if (NF(splt) < 1) return -1;

  else {
    Thunk thk = Thunk_lookup(update_cmds, F(splt, 0));
    if (!thk) return -1;
    else {
      int val;

      sem_wait(&top.sync.upd.sem);
      val = (*thk)(NF(splt) - 1, Fptr(splt, 1));
      sem_post(&top.sync.upd.sem);

      if (log && !quiet) {
	int i;
	char *s = since(&top.start_tv);
	fprintf(log, "update[%s]: returned %d from", s, val);
	for (i = 0; i < NF(splt); i++)
	  fprintf(log, " %s", F(splt, i));
	putc('\n', log);
	fflush(log);
      }

      return val;
    }
  }
}
