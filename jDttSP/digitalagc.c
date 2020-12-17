/* digitalagc.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2004 by Frank Brickle, AB2KT and Bob McGwier, N4HY

We appreciate the contributions to this subroutine by Gerald Youngblood,
AC5OG and Phil Harman, VK6APH.

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

void
DigitalAgc(DIGITALAGC a, int tick) {

  if (a->mode == agcOFF) {
    int i;
    for (i = 0; i < CXBsize(a->buff); i++)
      CXBdata(a->buff, i) = Cscl(CXBdata(a->buff, i), a->gain.fix);

  } else {
    int i, k, hang;
    REAL peak = 0.0;

    hang = tick < a->over ? a->rcov : a->hang;
    k = a->indx;

    for (i = 0; i < CXBsize(a->buff); i++)
      peak = max(peak, Cmag(CXBdata(a->buff, i)));

    if (peak != 0) {
      a->hist[a->indx] = a->gain.lim / peak;
      for (i = 1, a->gain.now = a->hist[0]; i < hang; i++)
	a->gain.now = min(a->hist[i], a->gain.now);
    }
    a->gain.raw = a->gain.now;
    a->gain.now = min(a->gain.now, a->gain.top);

    for (i = 0, k = (a->sndx + a->ramp) % a->mask;
	 i < CXBsize(a->buff);
	 i++, k = (k + 1) % a->mask)
      a->circ[k] = CXBdata(a->buff, i);

    if (a->gain.now != a->gain.old) {
      REAL step = (a->gain.now - a->gain.old) / a->ramp;
      for (i = 0, k = a->sndx;
	   i < a->ramp;
	   i++, k = (k + 1) % a->mask)
	CXBdata(a->buff, i) = Cscl(a->circ[k], a->gain.old + i * step);
      for (; i < CXBsize(a->buff); i++, k = (k + 1) % a->mask)
	CXBdata(a->buff, i) = Cscl(a->circ[k], a->gain.now);
      a->gain.old = a->gain.now;

    } else {
      for (i = 0, k = a->sndx;
	   i < CXBsize(a->buff);
	   i++, k = (k + 1) % a->mask)
	CXBdata(a->buff, i) = Cscl(a->circ[k], a->gain.now);
    }

    a->sndx = (a->sndx + CXBsize(a->buff)) % a->mask;
    a->indx = (a->indx + 1) % hang;
  }
}

/*
 * Mode is gross agc mode: off, slow, med, fast; just info
 * Hang is basic hang time
 * Size is max hang time
 * Over is recover period after TRX switch
 * Rcov is hang value used during recovery period
 * BufSize is length of sample buffer
 */

DIGITALAGC
newDigitalAgc(int Mode,
	      int Hang,
	      int Ramp,
	      int Over,
	      int Rcov,
	      int BufSize,
	      REAL MaxGain, REAL Limit, REAL CurGain, COMPLEX * Vec) {
  DIGITALAGC a = (DIGITALAGC) safealloc(1,
					sizeof(digital_agc_state),
					"new digital agc state");
  assert((Ramp >= 2) && (Ramp < BufSize));
  a->mode = Mode;
  a->hang = Hang;
  a->over = Over;
  a->rcov = Rcov;
  a->ramp = Ramp;
  a->gain.top = MaxGain;
  a->gain.lim = Limit;
  a->gain.old = a->gain.now = CurGain;
  a->buff = newCXB(BufSize, Vec, "agc buffer");
  a->mask = 2 * CXBsize(a->buff);
  a->circ = newvec_COMPLEX(2 * BufSize, "circular agc buffer");
  memset((void *) a->hist, 0, sizeof(a->hist));
  a->indx = 0;
  a->sndx = a->mask - Ramp;
  a->gain.fix = 10.0;
  return a;
}

void
delDigitalAgc(DIGITALAGC a) {
  if (a) {
    delCXB(a->buff);
    delvec_COMPLEX(a->circ);
    safefree((void *) a);
  }
}
