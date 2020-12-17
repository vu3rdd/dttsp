/* digitalagc.h

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

#ifndef _digitalagc_h
#define _digitalagc_h

#include <datatypes.h>
#include <bufvec.h>
#include <cxops.h>
#include <banal.h>

typedef enum _agcmode { agcOFF, agcLONG, agcSLOW, agcMED, agcFAST } AGCMODE;

#define AGCHIST (24)

typedef
struct _digitalagc {
  AGCMODE mode;
  int hang, indx, over, ramp, rcov, mask, sndx;
  struct {
    REAL fix, lim, now, old, raw, top;
  } gain;
  REAL hist[AGCHIST];
  CXB buff;
  COMPLEX *circ;
} digital_agc_state, *DIGITALAGC;

extern void delDigitalAgc(DIGITALAGC agc);

extern DIGITALAGC 
newDigitalAgc(int Mode,
	      int Hang,
	      int Ramp,
	      int Over,
	      int Rcov,
	      int BufSize,
	      REAL MaxGain,
	      REAL Limit,
	      REAL CurGain,
	      COMPLEX *Vec);

extern void DigitalAgc(DIGITALAGC agc, int tick);

#endif
