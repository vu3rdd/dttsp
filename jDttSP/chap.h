/* chap.h

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

#ifndef _chap_h
#define _chap_h

#include <fromsys.h>
#include <banal.h>
#include <splitfields.h>
#include <datatypes.h>
#include <bufvec.h>
#include <cxops.h>
#include <fastrig.h>
#include <update.h>
#include <lmadf.h>
#include <fftw.h>
#include <ovsv.h>
#include <filter.h>
#include <oscillator.h>

typedef struct
_cheby_approx {
  REAL (*func)(REAL);
  int len;
  REAL *coef;
  struct { REAL lo, hi, df; } lim;
  char *tag;
} ChApDesc, *ChAp;

#define ChApFunc(p) ((p)->func)
#define ChApLen(p) ((p)->len)
#define ChApLob(p) ((p)->lim.lo)
#define ChApHib(p) ((p)->lim.hi)
#define ChApDif(p) ((p)->lim.df)
#define ChApCoefBase(p) ((p)->coef)
#define ChApCoef(p, i) (((p)->coef)[i])
#define ChApTag(p) ((p)->tag)

extern REAL ChAp_eval(ChAp ca, REAL x, BOOLEAN *err);
extern ChAp ChAp_fit(ChAp ca);
extern ChAp newChAp(REAL (*func)(REAL arg),
		    REAL lo,
		    REAL hi,
		    int len,
		    char *tag);

extern void delChAp(ChAp ca);

#endif
