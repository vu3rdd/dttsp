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
  
#include <common.h>
//------------------------------------------------------------------------
/* modulation types, modes */ 
typedef enum _sdrmode {
  LSB,				//  0
  USB,				//  1
  DSB,				//  2
  CWL,				//  3
  CWU,				//  4
  FMN,				//  5
  AM,				//  6
  PSK,				//  7
  SPEC,				//  8
  RTTY,				//  9
  SAM,				// 10
  DRM				// 11
} SDRMODE;
typedef enum _trxmode { RX, TX } TRXMODE;

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

  struct {
    BOOLEAN flag;
    struct {
      char *path;
      size_t size;
      Chan c;
    } chan;
    REAL val, avgval;
    METERTYPE type;
  } meter;

  struct {
    BOOLEAN flag;
    PWS gen;
    struct {
      char *path;
      size_t size;
      Chan c;
    } chan;
    CXB buf;
    int fftsize;
  } powsp;

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

} uni;

//------------------------------------------------------------------------
/* RX */ 
//------------------------------------------------------------------------
extern struct _rx {
  struct {
    CXB i, o;
  } buf;
  struct {
    REAL freq, phase;
    OSC gen;
  } osc;
  struct {
    ComplexFIR coef;
    FiltOvSv ovsv;
    COMPLEX * save;
  } filt;
  IQ iqfix;
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
    struct {
      REAL meter, att, gain;
    } offset;
    BOOLEAN flag, running, set;
    int num;
  } squelch;
  SDRMODE mode;
  struct { BOOLEAN flag; } bin;
} rx;

//------------------------------------------------------------------------
/* TX */ 
//------------------------------------------------------------------------
extern struct _tx {
  struct {
    CXB i, o;
  } buf;
  struct {
    REAL freq, phase;
    OSC gen;
  } osc;
  struct {
    ComplexFIR coef;
    FiltOvSv ovsv;
    COMPLEX * save;
  } filt;
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
    SpeechProc gen;
    BOOLEAN flag;
  } spr;
  struct {
    COMPLEX dc;
    struct {
      REAL val;
      BOOLEAN flag;
    } pre, post;
  } scl;
  SDRMODE mode;
} tx;

//------------------------------------------------------------------------

typedef enum _runmode {
  RUN_MUTE, RUN_PASS, RUN_PLAY, RUN_SWCH 
} RUNMODE;

#ifndef _WINDOWS

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
    } buf;
    struct {
      unsigned int frames, bytes;
    } size;
  } hold;
  struct {
    char *path;
    int fd;
    FILE * fp;
    char buff[4096];
  } parm;
  
  struct {
    char name[256];
    jack_client_t * client;
    struct {
      struct {
	jack_port_t *l, *r;
      } i, o;
    } port;
    struct {
      struct {
	jack_ringbuffer_t *l, *r;
      } i, o;
    } ring;
  } jack;

  // update io
  // multiprocessing & synchronization
  struct {
    struct {
      pthread_t id;
    } trx, upd, pws, meter;
  } thrd;
  struct {
    struct {
      sem_t sem;
    } buf, upd;
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

#else

extern struct _top {
  pid_t pid;
  uid_t uid;

  BOOLEAN running, verbose;
  RUNMODE state;
  
  // audio io
  struct {
    struct {
      float *l, *r;
    } buf;
    struct {
      unsigned int frames, bytes;
    } size;
  } hold;
  struct {
    char *path;
    int fd;
    HANDLE *fpr;
    HANDLE *fpw;
    char buff[4096];
  } parm;
  
  struct {
    char name[256];
    struct {
      struct {
	ringb_t *l, *r;
      } i, o;
    } ring;
  } jack;

  // update io
  // multiprocessing & synchronization
  struct {
    struct {
      DWORD id;
      HANDLE thrd;
    } trx, upd, pws, meter;
  } thrd;
  struct {
    struct {
      HANDLE sem;
    } buf, upd, upd2;
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
