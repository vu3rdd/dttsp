/* sdr.c

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

//========================================================================
/* initialization and termination */

void
reset_meters(void) {  
  if (uni.meter.flag) { // reset metering completely
    int i, k;
    for (i = 0; i < RXMETERPTS; i++)
      for (k = 0; k < MAXRX; k++)
	uni.meter.rx.val[k][i] = uni.meter.rx.avg[k][i] = -200.0;
    for (i = 0; i < TXMETERPTS; i++)
      uni.meter.tx.val[i] = uni.meter.tx.avg[i] = -200.0;
  }
}

void
reset_spectrum(void) {  
  if (uni.spec.flag)
    reinit_spectrum(&uni.spec);
}

void
reset_counters(void) {
  int k;
  for (k = 0; k < uni.multirx.nrx; k++) rx[k].tick = 0;
  tx.tick = 0;
}

//========================================================================

/* global and general info,
   not specifically attached to
   tx, rx, or scheduling */

PRIVATE void
setup_all(void) {
  
  uni.samplerate = loc.def.rate;
  uni.buflen = loc.def.size;
  uni.mode.sdr = loc.def.mode;
  uni.mode.trx = RX;
  
  uni.wisdom.path = loc.path.wisdom;
  uni.wisdom.bits = FFTW_OUT_OF_PLACE | FFTW_ESTIMATE;
  {
    FILE *f = fopen(uni.wisdom.path, "r");
    if (f) {
#define WBUFLEN 2048
#define WSTRLEN 64      
      char *line = (char *)malloc(WBUFLEN);
      fgets(line, WBUFLEN, f);
      if ((strlen(line) > WSTRLEN) &&
	  (fftw_import_wisdom_from_string(line) != FFTW_FAILURE))
	uni.wisdom.bits = FFTW_OUT_OF_PLACE | FFTW_MEASURE | FFTW_USE_WISDOM;
#undef WSTRLEN
#undef WBUFLEN      
      fclose(f);
	  free(line);
    }

  }
  
  if (uni.meter.flag) {
    uni.meter.rx.type = SIGNAL_STRENGTH;
    uni.meter.tx.type = SIGNAL_STRENGTH;
    reset_meters();
  }
  
  uni.spec.rxk = 0;
  uni.spec.buflen = uni.buflen;
  uni.spec.scale = SPEC_PWR;
  uni.spec.type = SPEC_POST_FILT;
  uni.spec.size = loc.def.spec;
  uni.spec.planbits = uni.wisdom.bits;
  init_spectrum(&uni.spec);
  
  // set which receiver is listening to commands
  uni.multirx.lis = 0;
  uni.multirx.nrx = loc.def.nrx;
  
  // set mixing of input from aux ports
  uni.mix.rx.flag = uni.mix.tx.flag = FALSE;
  uni.mix.rx.gain = uni.mix.tx.gain = 1.0;
  
  uni.tick = 0;
}

/* purely rx */

PRIVATE void
setup_rx(int k) {
  
  /* conditioning */
  rx[k].iqfix = newCorrectIQ(0.0, 1.0);
  rx[k].filt.coef = newFIR_Bandpass_COMPLEX(-4800.0,
					    4800.0,
					    uni.samplerate,
					    uni.buflen + 1);
  rx[k].filt.ovsv = newFiltOvSv(FIRcoef(rx[k].filt.coef),
				FIRsize(rx[k].filt.coef),
				uni.wisdom.bits);
  normalize_vec_COMPLEX(rx[k].filt.ovsv->zfvec,
			rx[k].filt.ovsv->fftlen);

  // hack for EQ
  rx[k].filt.save = newvec_COMPLEX(rx[k].filt.ovsv->fftlen, "RX filter cache");
  memcpy((char *) rx[k].filt.save,
	 (char *) rx[k].filt.ovsv->zfvec,
	 rx[k].filt.ovsv->fftlen * sizeof(COMPLEX));

  /* buffers */
  /* note we overload the internal filter buffers
     we just created */
  rx[k].buf.i = newCXB(FiltOvSv_fetchsize(rx[k].filt.ovsv),
		       FiltOvSv_fetchpoint(rx[k].filt.ovsv),
		       "init rx.buf.i");
  rx[k].buf.o = newCXB(FiltOvSv_storesize(rx[k].filt.ovsv),
		       FiltOvSv_storepoint(rx[k].filt.ovsv),
		       "init rx[k].buf.o");
  
  /* conversion */
  rx[k].osc.freq = -11025.0;
  rx[k].osc.phase = 0.0;
  rx[k].osc.gen = newOSC(uni.buflen,
			 ComplexTone,
			 rx[k].osc.freq,
			 rx[k].osc.phase,
			 uni.samplerate,
			 "SDR RX Oscillator");

  rx[k].agc.gen = newDigitalAgc(agcMED,	// Mode
			     7,		// Hang
			     7,		// Size
			     48,	// Ramp
			     3,		// Over
			     3,		// Rcov
			     CXBsize(rx[k].buf.o),	// BufSize
			     100.0,	// MaxGain
			     0.707,	// Limit
			     1.0,	// CurGain
			     CXBbase(rx[k].buf.o));
  rx[k].agc.flag = TRUE;

  /* demods */
  rx[k].am.gen = newAMD(48000.0,	// REAL samprate
			0.0,	// REAL f_initial
			-500.0,	// REAL f_lobound,
			500.0,	// REAL f_hibound,
			400.0,	// REAL f_bandwid,
			CXBsize(rx[k].buf.o),	// int size,
			CXBbase(rx[k].buf.o),	// COMPLEX *ivec,
			CXBbase(rx[k].buf.o),	// COMPLEX *ovec,
			AMdet,	// AM Mode AMdet == rectifier,
		                //         SAMdet == synchronous detector
			"AM detector blew");	// char *tag
  rx[k].fm.gen = newFMD(48000,	// REAL samprate
			0.0,	// REAL f_initial
			-6000.0,	// REAL f_lobound
			6000.0,	// REAL f_hibound
			10000.0,	// REAL f_bandwid
			CXBsize(rx[k].buf.o),	// int size
			CXBbase(rx[k].buf.o),	// COMPLEX *ivec
			CXBbase(rx[k].buf.o),	// COMPLEX *ovec
			"New FM Demod structure");	// char *error message;

  /* noise reduction */
  rx[k].anf.gen = new_lmsr(rx[k].buf.o,	// CXB signal,
			   64,		// int delay,
			   0.01,		// REAL adaptation_rate,
			   0.00001,	// REAL leakage,
			   45,		// int adaptive_filter_size,
			   LMADF_INTERFERENCE);
  rx[k].anf.flag = FALSE;
  rx[k].anr.gen = new_lmsr(rx[k].buf.o,	// CXB signal,
			   64,		// int delay,
			   0.01,		// REAL adaptation_rate,
			   0.00001,	// REAL leakage,
			   45,		// int adaptive_filter_size,
			   LMADF_NOISE);
  rx[k].anr.flag = FALSE;

  rx[k].nb.thresh = 3.3;
  rx[k].nb.gen = new_noiseblanker(rx[k].buf.i, rx[k].nb.thresh);
  rx[k].nb.flag = FALSE;

  rx[k].nb_sdrom.thresh = 2.5;
  rx[k].nb_sdrom.gen = new_noiseblanker(rx[k].buf.i, rx[k].nb_sdrom.thresh);
  rx[k].nb_sdrom.flag = FALSE;

  rx[k].spot.gen = newSpotToneGen(-12.0,	// gain
				  700.0,	// freq
				  5.0,	// ms rise
				  5.0,	// ms fall
				  uni.buflen,
				  uni.samplerate);

  rx[k].scl.pre.val = 1.0;
  rx[k].scl.pre.flag = FALSE;
  rx[k].scl.post.val = 1.0;
  rx[k].scl.post.flag = FALSE;

  memset((char *) &rx[k].squelch, 0, sizeof(rx[k].squelch));
  rx[k].squelch.thresh = -30.0;
  rx[k].squelch.power = 0.0;
  rx[k].squelch.flag = rx[k].squelch.running = rx[k].squelch.set = FALSE;
  rx[k].squelch.num = (int) (0.0395 * uni.samplerate + 0.5);

  rx[k].mode = uni.mode.sdr;
  rx[k].bin.flag = FALSE;

  {
    REAL pos = 0.5, // 0 <= pos <= 1, left->right
         theta = (1.0 - pos) * M_PI / 2.0;
    rx[k].azim = Cmplx(cos(theta), sin(theta));
  }

  rx[k].tick = 0;
}

/* purely tx */

PRIVATE void
setup_tx(void) {

  /* conditioning */
  tx.iqfix = newCorrectIQ(0.0, 1.0);
  tx.filt.coef = newFIR_Bandpass_COMPLEX(300.0,
					 3000.0,
					 uni.samplerate,
					 uni.buflen + 1);
  tx.filt.ovsv = newFiltOvSv(FIRcoef(tx.filt.coef),
			     FIRsize(tx.filt.coef),
			     uni.wisdom.bits);
  normalize_vec_COMPLEX(tx.filt.ovsv->zfvec,
			tx.filt.ovsv->fftlen);

  // hack for EQ
  tx.filt.save = newvec_COMPLEX(tx.filt.ovsv->fftlen, "TX filter cache");
  memcpy((char *) tx.filt.save,
	 (char *) tx.filt.ovsv->zfvec,
	 tx.filt.ovsv->fftlen * sizeof(COMPLEX));

  /* buffers */
  tx.buf.i = newCXB(FiltOvSv_fetchsize(tx.filt.ovsv),
		    FiltOvSv_fetchpoint(tx.filt.ovsv),
		    "init tx.buf.i");
  tx.buf.o = newCXB(FiltOvSv_storesize(tx.filt.ovsv),
		    FiltOvSv_storepoint(tx.filt.ovsv),
		    "init tx.buf.o");
  
  /* conversion */
  tx.osc.freq = 0.0;
  tx.osc.phase = 0.0;
  tx.osc.gen = newOSC(uni.buflen,
		      ComplexTone,
		      tx.osc.freq,
		      tx.osc.phase,
		      uni.samplerate,
		      "SDR TX Oscillator");

  tx.agc.gen = newDigitalAgc(agcFAST, 	// Mode
			     3,		// Hang
			     3,		// Size
			     3,		// Over
			     3,		// Rcov
			     48,	// Ramp
			     CXBsize(tx.buf.o),	// BufSize
			     1.0,	// MaxGain
			     0.900,	// Limit
			     1.0,	// CurGain
			     CXBbase(tx.buf.o));
  tx.agc.flag = TRUE;

  tx.spr.gen = newSpeechProc(0.4, 10.0, CXBbase(tx.buf.i), CXBsize(tx.buf.i));
  tx.spr.flag = FALSE;

  tx.scl.dc = cxzero;
  tx.scl.pre.val = 1.0;
  tx.scl.pre.flag = FALSE;
  tx.scl.post.val = 1.0;
  tx.scl.post.flag = FALSE;

  tx.mode = uni.mode.sdr;

  tx.tick = 0;
  /* not much else to do for TX */
}

/* how the outside world sees it */

void
setup_workspace(void) {
  int k;

  setup_all();

  for (k = 0; k < uni.multirx.nrx; k++) {
    setup_rx(k);
    uni.multirx.act[k] = FALSE;
  }
  uni.multirx.act[0] = TRUE;
  uni.multirx.nac = 1;
  
  setup_tx();
}

void
destroy_workspace(void) {
  int k;

  /* TX */
  delSpeechProc(tx.spr.gen);
  delDigitalAgc(tx.agc.gen);
  delOSC(tx.osc.gen);
  delvec_COMPLEX(tx.filt.save);
  delFiltOvSv(tx.filt.ovsv);
  delFIR_Bandpass_COMPLEX(tx.filt.coef);
  delCorrectIQ(tx.iqfix);
  delCXB(tx.buf.o);
  delCXB(tx.buf.i);

  /* RX */
  for (k = 0; k < uni.multirx.nrx; k++) {
    delSpotToneGen(rx[k].spot.gen);
    delDigitalAgc(rx[k].agc.gen);
    del_nb(rx[k].nb_sdrom.gen);
    del_nb(rx[k].nb.gen);
    del_lmsr(rx[k].anf.gen);
    del_lmsr(rx[k].anr.gen);
    delAMD(rx[k].am.gen);
    delFMD(rx[k].fm.gen);
    delOSC(rx[k].osc.gen);
    delvec_COMPLEX(rx[k].filt.save);
    delFiltOvSv(rx[k].filt.ovsv);
    delFIR_Bandpass_COMPLEX(rx[k].filt.coef);
    delCorrectIQ(rx[k].iqfix);
    delCXB(rx[k].buf.o);
    delCXB(rx[k].buf.i);
  }
  
  /* all */
  finish_spectrum(&uni.spec);
}

//////////////////////////////////////////////////////////////////////////
// execution
//////////////////////////////////////////////////////////////////////////

//========================================================================
// util

PRIVATE REAL
CXBnorm(CXB buff) {
  int i;
  double sum = 0.0;
  for (i = 0; i < CXBhave(buff); i++)
    sum += Csqrmag(CXBdata(buff, i));
  return sqrt(sum);
}

//========================================================================
/* all */

// unfortunate duplication here, due to
// multirx vs monotx

PRIVATE void
do_rx_meter(int k, CXB buf, int tap) {
  COMPLEX *vec = CXBbase(buf);
  int i, len = CXBhave(buf);
  
  uni.meter.rx.val[k][tap] = 0;
  
  switch (uni.meter.rx.type) {
  case AVG_SIGNAL_STRENGTH:
    for (i = 0; i < len; i++)
      uni.meter.rx.val[k][tap] += Csqrmag(vec[i]);
    uni.meter.rx.val[k][tap] =
      uni.meter.rx.avg[k][tap] =
        0.9 * uni.meter.rx.avg[k][tap] + log10(uni.meter.rx.val[k][tap] + 1e-20);
    break;
  case SIGNAL_STRENGTH:
    for (i = 0; i < len; i++)
      uni.meter.rx.val[k][tap] += Csqrmag(vec[i]);
    uni.meter.rx.avg[k][tap] =
      uni.meter.rx.val[k][tap] =
        10.0 * log10(uni.meter.rx.val[k][tap] + 1e-20);
    break;
  case ADC_REAL:
    for(i = 0; i < len; i++)
      uni.meter.rx.val[k][tap] = max(fabs(vec[i].re), uni.meter.rx.val[k][tap]);
    uni.meter.rx.val[k][tap] = 20.0 * log10(uni.meter.rx.val[k][tap] + 1e-10);
    break;
  case ADC_IMAG:
    for(i = 0; i < len; i++)
      uni.meter.rx.val[k][tap] = max(fabs(vec[i].im), uni.meter.rx.val[k][tap]);
    uni.meter.rx.val[k][tap] = 20.0 * log10(uni.meter.rx.val[k][tap] + 1e-10);
    break;
  default:
    break;
  }
}

PRIVATE void
do_tx_meter(CXB buf, int tap) {
  COMPLEX *vec = CXBbase(buf);
  int i, len = CXBhave(buf);
  
  uni.meter.tx.val[tap] = 0;

  switch (uni.meter.tx.type) {
  case AVG_SIGNAL_STRENGTH:
    for (i = 0; i < len; i++)
      uni.meter.tx.val[tap] += Csqrmag(vec[i]);
    uni.meter.tx.val[tap] =
      uni.meter.tx.avg[tap] =
        0.9 * uni.meter.tx.avg[tap] + log10(uni.meter.tx.val[tap] + 1e-20);
    break;
  case SIGNAL_STRENGTH:
    for (i = 0; i < len; i++)
      uni.meter.tx.val[tap] += Csqrmag(vec[i]);
    uni.meter.tx.avg[tap] =
      uni.meter.tx.val[tap] =
        10.0 * log10(uni.meter.tx.val[tap] + 1e-20);
    break;
  case ADC_REAL:
    for(i = 0; i < len; i++)
      uni.meter.tx.val[tap] = max(fabs(vec[i].re), uni.meter.tx.val[tap]);
    uni.meter.tx.val[tap] = 20.0 * log10(uni.meter.tx.val[tap] + 1e-10);
    break;
  case ADC_IMAG:
    for(i = 0; i < len; i++)
      uni.meter.tx.val[tap] = max(fabs(vec[i].im), uni.meter.tx.val[tap]);
    uni.meter.tx.val[tap] = 20.0 * log10(uni.meter.tx.val[tap] + 1e-10);
    break;
  default:
    break;
  }
}

PRIVATE void
do_rx_spectrum(int k, CXB buf, int type) {
  if (uni.spec.flag && k == uni.spec.rxk && type == uni.spec.type) {
    memcpy((char *) &CXBdata(uni.spec.accum, uni.spec.fill),
	   (char *) CXBbase(buf),
	   CXBhave(buf)); 
    uni.spec.fill = (uni.spec.fill + uni.spec.buflen) % uni.spec.size;
  }
}

PRIVATE void
do_tx_spectrum(CXB buf) {
  memcpy((char *) &CXBdata(uni.spec.accum, uni.spec.fill),
	 (char *) CXBbase(buf),
	 CXBhave(buf));
  uni.spec.fill = (uni.spec.fill + uni.spec.buflen) % uni.spec.size;
}

//========================================================================
/* RX processing */ 

PRIVATE BOOLEAN
should_do_rx_squelch(int k) {
  if (rx[k].squelch.flag) {
    int i, n = CXBhave(rx[k].buf.o);
    rx[k].squelch.power = 0.0;
    for (i = 0; i < n; i++)
      rx[k].squelch.power += Csqrmag(CXBdata(rx[k].buf.o, i));
    return rx[k].squelch.thresh > 10.0 * log10(rx[k].squelch.power);
  } else
    return rx[k].squelch.set = FALSE;
}

// apply squelch
// slew into silence first time

PRIVATE void
do_squelch(int k) {
  rx[k].squelch.set = TRUE;
  if (!rx[k].squelch.running) {
    int i, m = rx[k].squelch.num, n = CXBhave(rx[k].buf.o) - m;
    for (i = 0; i < m; i++)
      CXBdata(rx[k].buf.o, i) = Cscl(CXBdata(rx[k].buf.o, i), 1.0 - (REAL) i / m);
    memset((void *) (CXBbase(rx[k].buf.o) + m), 0, n * sizeof(COMPLEX));
    rx[k].squelch.running = TRUE;
  } else
    memset((void *) CXBbase(rx[k].buf.o), 0, CXBhave(rx[k].buf.o) * sizeof(COMPLEX));
}

// lift squelch
// slew out from silence to full scale

PRIVATE void
no_squelch(int k) {
  if (rx[k].squelch.running) {
    int i, m = rx[k].squelch.num;
    for (i = 0; i < m; i++)
      CXBdata(rx[k].buf.o, i) = Cscl(CXBdata(rx[k].buf.o, i), (REAL) i / m);
    rx[k].squelch.running = FALSE;
  }
}

/* pre-condition for (nearly) all RX modes */

PRIVATE void
do_rx_pre(int k) {
  int i, n = min(CXBhave(rx[k].buf.i), uni.buflen);

  if (rx[k].scl.pre.flag)
    for (i = 0; i < n; i++)
      CXBdata(rx[k].buf.i, i) = Cscl(CXBdata(rx[k].buf.i, i),
				     rx[k].scl.pre.val); 

  if (rx[k].nb.flag) noiseblanker(rx[k].nb.gen);
  if (rx[k].nb_sdrom.flag) SDROMnoiseblanker(rx[k].nb_sdrom.gen);

  // metering for uncorrected values here

  do_rx_meter(k, rx[k].buf.i, RXMETER_PRE_CONV);

  correctIQ(rx[k].buf.i, rx[k].iqfix);

  /* 2nd IF conversion happens here */

  if (rx[k].osc.gen->Frequency != 0.0) {
    ComplexOSC(rx[k].osc.gen);
    for (i = 0; i < n; i++)
      CXBdata(rx[k].buf.i, i) = Cmul(CXBdata(rx[k].buf.i, i),
				     OSCCdata(rx[k].osc.gen, i));
  } 

  /* filtering, metering, spectrum, squelch, & AGC */
  
  if (rx[k].mode == SPEC)
    
    do_rx_spectrum(k, rx[k].buf.i, SPEC_SEMI_RAW);
  
  else {
    
    do_rx_meter(k, rx[k].buf.i, RXMETER_PRE_FILT);
    do_rx_spectrum(k, rx[k].buf.i, SPEC_PRE_FILT);
    
    if (rx[k].tick == 0)
      reset_OvSv(rx[k].filt.ovsv);
    
    filter_OvSv(rx[k].filt.ovsv);
    CXBhave(rx[k].buf.o) = CXBhave(rx[k].buf.i);
    
    do_rx_meter(k, rx[k].buf.o, RXMETER_POST_FILT);
    do_rx_spectrum(k, rx[k].buf.o, SPEC_POST_FILT);
    
    if (should_do_rx_squelch(k))
      do_squelch(k);
    
    else if (rx[k].agc.flag)
      DigitalAgc(rx[k].agc.gen, rx[k].tick);
    
  }
}

PRIVATE void
do_rx_post(int k) {
  int i, n = CXBhave(rx[k].buf.o);
  
  if (!rx[k].squelch.set)  {
    no_squelch(k);
    // spotting tone
    if (rx[k].spot.flag) {
      // remember whether it's turned itself off during this pass
      rx[k].spot.flag = SpotTone(rx[k].spot.gen);
      for (i = 0; i < n; i++)
	CXBdata(rx[k].buf.o, i) = Cadd(CXBdata(rx[k].buf.o, i),
				       CXBdata(rx[k].spot.gen->buf, i));
    }
  }
  
  // final scaling
  
  if (rx[k].scl.post.flag)
    for (i = 0; i < n; i++)
      CXBdata(rx[k].buf.o, i) = Cscl(CXBdata(rx[k].buf.o, i),
				     rx[k].scl.post.val);
  
  // not binaural?
  // position in stereo field
  
  if (!rx[k].bin.flag)
    for (i = 0; i < n; i++)
      CXBdata(rx[k].buf.o, i) = Cscl(rx[k].azim, CXBreal(rx[k].buf.o, i));
}

/* demod processing */

PRIVATE void
do_rx_SBCW(int k) {
  if (rx[k].anr.flag) lmsr_adapt(rx[k].anr.gen);
  if (rx[k].anf.flag) lmsr_adapt(rx[k].anf.gen);
}

PRIVATE void
do_rx_AM(int k) { AMDemod(rx[k].am.gen); }

PRIVATE void
do_rx_FM(int k) { FMDemod(rx[k].fm.gen); }

PRIVATE void
do_rx_DRM(int k) {}

PRIVATE void
do_rx_SPEC(int k) {
  memcpy(CXBbase(rx[k].buf.o),
	 CXBbase(rx[k].buf.i),
	 sizeof(COMPLEX) * CXBhave(rx[k].buf.i));
  if (rx[k].agc.flag) DigitalAgc(rx[k].agc.gen, rx[k].tick);
}

PRIVATE void
do_rx_NIL(int k) {
  int i, n = min(CXBhave(rx[k].buf.i), uni.buflen);
  for (i = 0; i < n; i++) CXBdata(rx[k].buf.o, i) = cxzero;
}

/* overall dispatch for RX processing */

PRIVATE void
do_rx(int k) {
  do_rx_pre(k);
  switch (rx[k].mode) {
  case USB:
  case LSB:
  case CWU:
  case CWL:
  case DSB:  do_rx_SBCW(k); break;
  case AM:
  case SAM:  do_rx_AM(k); break;
  case FMN:  do_rx_FM(k);   break;
  case DRM:  do_rx_DRM(k);  break;
  case SPEC:
    default: do_rx_SPEC(k); break;
  }
  do_rx_post(k);
}  

//==============================================================
/* TX processing */

/* pre-condition for (nearly) all TX modes */

PRIVATE void
do_tx_pre(void) {

if (tx.scl.pre.flag) {
int i, n = CXBhave(tx.buf.i);
    for (i = 0; i < n; i++)
      CXBdata(tx.buf.i, i) = Cmplx(CXBreal(tx.buf.i, i) * tx.scl.pre.val, 0.0);
  }

  correctIQ(tx.buf.i, tx.iqfix);

  if (tx.spr.flag) SpeechProcessor(tx.spr.gen);

  if (tx.tick == 0) reset_OvSv(tx.filt.ovsv);
  filter_OvSv(tx.filt.ovsv);
}

PRIVATE void
do_tx_post(void) {
  CXBhave(tx.buf.o) = CXBhave(tx.buf.i);

  if (tx.agc.flag) DigitalAgc(tx.agc.gen, tx.tick);

  // meter modulated signal

  do_tx_meter(tx.buf.o, TXMETER_POST_MOD);

  if (tx.scl.post.flag) {
    int i, n = CXBhave(tx.buf.o);
    for (i = 0; i < n; i++)
      CXBdata(tx.buf.o, i) = Cscl(CXBdata(tx.buf.o, i), tx.scl.post.val);
  }

  if (uni.spec.flag)
    do_tx_spectrum(tx.buf.o);

  if (tx.osc.gen->Frequency != 0.0) {
    int i;
    ComplexOSC(tx.osc.gen);
    for (i = 0; i < CXBhave(tx.buf.o); i++)
      CXBdata(tx.buf.o, i) = Cmul(CXBdata(tx.buf.o, i), OSCCdata(tx.osc.gen, i));
  }
}

/* modulator processing */

PRIVATE void
do_tx_SBCW(void) {
  int i, n = min(CXBhave(tx.buf.o), uni.buflen); 

  if ((tx.norm = CXBnorm(tx.buf.o)) > 0.0)
    for (i = 0; i < n; i++) {
      tx.scl.dc = Cadd(Cscl(tx.scl.dc, 0.99),
		       Cscl(CXBdata(tx.buf.o, i), -0.01));
      CXBdata(tx.buf.o, i) = Cadd(CXBdata(tx.buf.o, i), tx.scl.dc);
    }
}

PRIVATE void
do_tx_AM(void) {
  int i, n = min(CXBhave(tx.buf.o), uni.buflen); 

  if ((tx.norm = CXBnorm(tx.buf.o)) > 0.0)
    for (i = 0; i < n; i++) { 
      tx.scl.dc = Cadd(Cscl(tx.scl.dc, 0.999),
		       Cscl(CXBdata(tx.buf.o, i), -0.001));
      CXBreal(tx.buf.o, i) =
	0.49995 + 0.49995 * (CXBreal(tx.buf.o, i) - tx.scl.dc.re);
      CXBimag(tx.buf.o, i) = 0.0;
    }
}

PRIVATE void
do_tx_FM(void) {
  int i, n = min(CXBhave(tx.buf.o), uni.buflen);
  if ((tx.norm = CXBnorm(tx.buf.o)) > 0.0)
    for (i = 0; i < n; i++) {
      tx.scl.dc = Cadd(Cscl(tx.scl.dc, 0.999),
		       Cscl(CXBdata(tx.buf.o, i), 0.001));
      tx.osc.phase += (CXBreal(tx.buf.o, i) - tx.scl.dc.re) * CvtMod2Freq;
      if (tx.osc.phase >= TWOPI) tx.osc.phase -= TWOPI;
      if (tx.osc.phase < 0.0) tx.osc.phase += TWOPI;
      CXBdata(tx.buf.o, i) =
	Cscl(Cmplx(cos(tx.osc.phase), sin(tx.osc.phase)), 0.99999);
    }
}

PRIVATE void
do_tx_NIL(void) {
  int i, n = min(CXBhave(tx.buf.i), uni.buflen);
  for (i = 0; i < n; i++) CXBdata(tx.buf.o, i) = cxzero;
}

/* general TX processing dispatch */

PRIVATE void
do_tx(void) {
  do_tx_pre();
  switch (tx.mode) {
  case USB:
  case LSB:
  case CWU:
  case CWL:
  case DSB:  do_tx_SBCW(); break;
  case AM:
  case SAM:  do_tx_AM();   break;
  case FMN:  do_tx_FM();   break;
  case DRM:
  case SPEC:
    default: do_tx_NIL(); break;
  }
  do_tx_post();
}

//========================================================================
/* overall buffer processing;
   come here when there are buffers to work on */

void
process_samples(float *bufl, float *bufr,
		float *auxl, float *auxr,
		int n) {
  int i, k;
  
  switch (uni.mode.trx) {
    
  case RX:
    
    // make copies of the input for all receivers
    for (k = 0; k < uni.multirx.nrx; k++)
      if (uni.multirx.act[k]) {
	for (i = 0; i < n; i++)
	  CXBimag(rx[k].buf.i, i) = bufl[i], CXBreal(rx[k].buf.i, i) = bufr[i];
	CXBhave(rx[k].buf.i) = n;
      }

    // prepare buffers for mixing
    memset((char *) bufl, 0, n * sizeof(float));
    memset((char *) bufr, 0, n * sizeof(float));

    // run all receivers
    for (k = 0; k < uni.multirx.nrx; k++)
      if (uni.multirx.act[k]) {
	do_rx(k), rx[k].tick++;
	// mix
	for (i = 0; i < n; i++)
          bufl[i] += (float)CXBimag(rx[k].buf.o, i),
	  bufr[i] += (float)CXBreal(rx[k].buf.o, i);
	CXBhave(rx[k].buf.o) = n;
      }

    // late mixing of aux buffers
    if (uni.mix.rx.flag)
      for (i = 0; i < n; i++)
	bufl[i] += (float)(auxl[i] * uni.mix.rx.gain),
	bufr[i] += (float)(auxr[i] * uni.mix.rx.gain);

    break;

  case TX:

    // early mixing of aux buffers
    if (uni.mix.tx.flag)
      for (i = 0; i < n; i++)
	bufl[i] += (float)(auxl[i] * uni.mix.tx.gain),
	bufr[i] += (float)(auxr[i] * uni.mix.tx.gain);

    for (i = 0; i < n; i++)
      CXBimag(tx.buf.i, i) = bufl[i], CXBreal(tx.buf.i, i) = bufr[i];
    CXBhave(tx.buf.i) = n;

    do_tx(), tx.tick++;

    for (i = 0; i < n; i++)
      bufl[i] = (float) CXBimag(tx.buf.o, i), bufr[i] = (float) CXBreal(tx.buf.o, i);
    CXBhave(tx.buf.o) = n;

    break;
  }

  uni.tick++;
}
