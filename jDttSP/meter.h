/* meter.h */
/*
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

#ifndef _meter_h
#define _meter_h

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

typedef enum {
  SIGNAL_STRENGTH, 
  AVG_SIGNAL_STRENGTH, 
  ADC_REAL, 
  ADC_IMAG,
  AGC_GAIN,
  ALC,  
  PWR,
  PKPWR
} METERTYPE;

#define RXMETERPTS (3)
#define RXMETER_PRE_CONV (0)
#define RXMETER_PRE_FILT (1)
#define RXMETER_POST_FILT (2)

#define TXMETERPTS (1)
#define TXMETER_POST_MOD (0)

typedef
struct _meter_block {
  BOOLEAN flag;
  int label;
  struct {
    METERTYPE type;
    REAL val[MAXRX][RXMETERPTS],
         avg[MAXRX][RXMETERPTS];
  } rx;
  struct {
    METERTYPE type;
    REAL val[TXMETERPTS],
         avg[TXMETERPTS];
  } tx;
  struct {
    REAL rx[MAXRX * RXMETERPTS],
         tx[TXMETERPTS];
  } snap;
} MeterBlock;

extern void snap_meter(MeterBlock *mb, int label);

#endif
