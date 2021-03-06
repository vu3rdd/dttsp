/* sdrexport.h

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
  
#ifndef _sdrexport_h
#define _sdrexport_h
  
#include <fromsys.h>
#include <defs.h>
#include <banal.h>
#include <splitfields.h>
#include <datatypes.h>
#include <bufvec.h>
#include <cxops.h>
#include <ringb.h>
#include <chan.h>
#include <lmadf.h>
#include <fftw.h>
#include <ovsv.h>
#include <filter.h>
#include <oscillator.h>
#include <digitalagc.h>
#include <am_demod.h>
#include <fm_demod.h>
#include <noiseblanker.h>
#include <correctIQ.h>
#include <crc16.h>
#include <speechproc.h>
#include <spottone.h>
#include <update.h>
#include <local.h>
#include <meter.h>
#include <spectrum.h>

//------------------------------------------------------------------------
// max no. simultaneous receivers
#ifndef MAXRX
#define MAXRX (4)
#endif

//========================================================================
/* RX/TX both */ 
//------------------------------------------------------------------------
extern struct _uni {
  REAL samplerate;
  int buflen;

  struct {
    SDRMODE sdr;
    TRXMODE trx;
  } mode;

  MeterBlock meter;
  SpecBlock spec;

  struct {
    BOOLEAN flag;
    struct {
      char *path;
      size_t size;
      Chan c;
    } chan;
    splitfld splt;
  } update;

  struct {
    char *path;
    int bits;
  } wisdom;

  struct {
    BOOLEAN act[MAXRX];
    int lis, nac, nrx;
  } multirx;

  struct {
    struct {
      BOOLEAN flag;
      REAL gain;
    } rx, tx;
  } mix;

  int cpdlen;

  long tick;
  
} uni;

//------------------------------------------------------------------------
/* RX */ 
//------------------------------------------------------------------------

extern struct _rx {
  struct {
    CXB i, o;
  } buf;
  IQ iqfix;
  struct {
    REAL freq, phase;
    OSC gen;
  } osc;
  struct {
    ComplexFIR coef;
    FiltOvSv ovsv;
    COMPLEX *save;
  } filt;
  struct {
    REAL thresh;
    NB gen;
    BOOLEAN flag;
  } nb;
  struct {
    REAL thresh;
    NB gen;
    BOOLEAN flag;
  } nb_sdrom;
  struct {
    LMSR gen;
    BOOLEAN flag;
  } anr, anf;
  struct {
    DIGITALAGC gen;
    BOOLEAN flag;
  } agc;
  struct { AMD gen; } am;
  struct { FMD gen; } fm;
  struct {
    BOOLEAN flag;
    SpotToneGen gen;
  } spot;
  struct {
    struct {
      REAL val;
      BOOLEAN flag;
    } pre, post;
  } scl;
  struct {
    REAL thresh, power;
    BOOLEAN flag, running, set;
    int num;
  } squelch;

  struct {
    BOOLEAN flag;
    WSCompander gen;
  } cpd;

  SDRMODE mode;
  struct { BOOLEAN flag; } bin;
  REAL norm;
  COMPLEX azim;
  long tick;
} rx[MAXRX];

//------------------------------------------------------------------------
/* TX */ 
//------------------------------------------------------------------------
extern struct _tx {

  struct {
    CXB i, o;
  } buf;

  IQ iqfix;

  struct {
    BOOLEAN flag;
    DCBlocker gen;
  } dcb;

  struct {
    REAL freq, phase;
    OSC gen;
  } osc;

  struct {
    ComplexFIR coef;
    FiltOvSv ovsv;
    COMPLEX *save;
  } filt;

  struct {
    SpeechProc gen;
    BOOLEAN flag;
  } spr;

  struct {
    BOOLEAN flag;
    WSCompander gen;
  } cpd;

  struct {
    ComplexFIR coef;
    FiltOvSv ovsv;
    CXB in, out;
  } fm;

  struct {
    DIGITALAGC gen;
    BOOLEAN flag;
  } agc;

  struct {
    COMPLEX dc;
    struct {
      REAL val;
      BOOLEAN flag;
    } pre, post;
  } scl;

  SDRMODE mode;
  long tick;
  REAL norm;
} tx;

//------------------------------------------------------------------------

typedef enum _runmode {
  RUN_MUTE, RUN_PASS, RUN_PLAY, RUN_SWCH 
} RUNMODE;

extern struct _top {
  pid_t pid;
  uid_t uid;
  
  struct timeval start_tv;

  BOOLEAN running, verbose;
  RUNMODE state;
  
  // audio io
  struct {
    struct {
      float *l, *r;
    } aux, buf;
    struct {
      unsigned int frames, bytes;
    } size;
  } hold;

  struct {
    char *path;
    int fd;
    FILE *fp;
    char buff[4096];
  } parm;

  struct {
    struct {
      char *path;
      FILE *fp;
    } mtr, spec;
  } meas;
  
  struct {
    char name[256];
    jack_client_t *client;
    struct {
      struct {
	jack_port_t *l, *r;
      } i, o;
    } port;

    // input only
    struct {
      struct {
	jack_port_t *l, *r;
      } i;
    } auxp;

    struct {
      struct {
	jack_ringbuffer_t *l, *r;
      } i, o;
    } ring;

    struct {
      struct {
	jack_ringbuffer_t *l, *r;
      } i, o;
    } auxr;

    jack_nframes_t size;
    struct {
      int cb;
      struct {
	int i, o;
      } rb;
      int xr;
    } blow;
  } jack;

  // update io
  // multiprocessing & synchronization
  struct {
    struct {
      pthread_t id;
    } trx, upd, mon, pws, mtr;
  } thrd;

  struct {
    struct {
      sem_t sem;
    } buf, upd, mon, pws, mtr;
  } sync;
  
  // TRX switching
  struct {
    struct {
      int want, have;
    } bfct;
    struct {
      TRXMODE next;
    } trx;
    struct {
      RUNMODE last;
    } run;
    int fade, tail;
  } swch;

} top;

#endif
