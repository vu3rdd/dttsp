/* lmadf.c 

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

#include <lmadf.h>

#ifdef REALLMS

LMSR 
new_lmsr(CXB signal,
	 int delay,
	 REAL adaptation_rate,
	 REAL leakage,
	 int adaptive_filter_size,
	 int filter_type) {
  LMSR lms = (LMSR) safealloc(1, sizeof(_lmsstate), "new_lmsr state");

  lms->signal = signal;
  lms->signal_size = CXBsize(lms->signal);
  lms->delay = delay;
  lms->size = 512;
  lms->mask = lms->size - 1;
  lms->delay_line = newvec_REAL(lms->size, "lmsr delay");
  lms->adaptation_rate = adaptation_rate;
  lms->leakage = leakage;
  lms->adaptive_filter_size = adaptive_filter_size;
  lms->adaptive_filter = newvec_REAL(128, "lmsr filter");
  lms->filter_type = filter_type;
  lms->delay_line_ptr = 0;

  return lms;
}

void
del_lmsr(LMSR lms) {
  if (lms) {
    delvec_REAL(lms->delay_line);
    delvec_REAL(lms->adaptive_filter);
    safefree((char *) lms);
  }
}

// just to make the algorithm itself a little clearer,
// get the admin stuff out of the way

#define ssiz (lms->signal_size)
#define asiz (lms->adaptive_filter_size)
#define dptr (lms->delay_line_ptr)
#define rate (lms->adaptation_rate)
#define leak (lms->leakage)

#define ssig(n) (CXBreal(lms->signal,(n)))

#define dlay(n) (lms->delay_line[(n)])

#define afil(n) (lms->adaptive_filter[(n)])
#define wrap(n) (((n) + (lms->delay) + (lms->delay_line_ptr)) & (lms->mask))
#define bump(n) (((n) + (lms->mask)) & (lms->mask))

static void
lmsr_adapt_i(LMSR lms) {
  int i, j, k;
  REAL sum_sq, scl1, scl2;
  REAL accum, error;

  scl1 = 1.0 - rate * leak;

  for (i = 0; i < ssiz; i++) {

    dlay(dptr) = ssig(i);
    accum = 0.0;
    sum_sq = 0.0;

    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      sum_sq += sqr(dlay(k));
      accum += afil(j) * dlay(k);
    }

    error = ssig(i) - accum;
    ssig(i) = error;

    scl2 = rate / (sum_sq + 1e-10);
    error *= scl2;
    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      afil(j) = afil(j) * scl1 + error * dlay(k);
    }

    dptr = bump(dptr);
  }
}

static void
lmsr_adapt_n(LMSR lms) {
  int i, j, k;
  REAL sum_sq, scl1, scl2;
  REAL accum, error;

  scl1 = 1.0 - rate * leak;

  for (i = 0; i < ssiz; i++) {

    dlay(dptr) = ssig(i);
    accum = 0.0;
    sum_sq = 0.0;

    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      sum_sq += sqr(dlay(k));
      accum += afil(j) * dlay(k);
    }

    error = ssig(i) - accum;
    ssig(i) = accum;

    scl2 = rate / (sum_sq + 1e-10);
    error *= scl2;
    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      afil(j) = afil(j) * scl1 + error * dlay(k);
    }

    dptr = bump(dptr);
  }
}

#else

LMSR 
new_lmsr(CXB signal,
	 int delay,
	 REAL adaptation_rate,
	 REAL leakage,
	 int adaptive_filter_size,
	 int filter_type) {
  LMSR lms = (LMSR) safealloc(1, sizeof(_lmsstate), "new_lmsr state");

  lms->signal = signal;
  CXBsize(lms->signal);
  lms->delay = delay;
  lms->size = 512;
  lms->mask = lms->size - 1;
  lms->delay_line = newvec_COMPLEX(lms->size, "lmsr delay");
  lms->adaptation_rate = adaptation_rate;
  lms->leakage = leakage;
  lms->adaptive_filter_size = adaptive_filter_size;
  lms->adaptive_filter = newvec_COMPLEX(128, "lmsr filter");
  lms->filter_type = filter_type;
  lms->delay_line_ptr = 0;

  return lms;
}

void
del_lmsr(LMSR lms) {
  if (lms) {
    delvec_COMPLEX(lms->delay_line);
    delvec_COMPLEX(lms->adaptive_filter);
    safefree((char *) lms);
  }
}

// just to make the algorithm itself a little clearer,
// get the admin stuff out of the way

#define ssiz (lms->signal_size)
#define asiz (lms->adaptive_filter_size)
#define dptr (lms->delay_line_ptr)
#define rate (lms->adaptation_rate)
#define leak (lms->leakage)

#define ssig(n) (CXBdata(lms->signal,(n)))

#define dlay(n) (lms->delay_line[(n)])

#define afil(n) (lms->adaptive_filter[(n)])
#define wrap(n) (((n) + (lms->delay) + (lms->delay_line_ptr)) & (lms->mask))
#define bump(n) (((n) + (lms->mask)) & (lms->mask))

static void
lmsr_adapt_i(LMSR lms) {
  int i, j, k;
  REAL sum_sq, scl1, scl2;
  COMPLEX accum, error;
  scl1 = 1.0 - rate * leak;
  for (i = 0; i < ssiz; i++) {

    dlay(dptr) = ssig(i);
    accum = cxzero;
    sum_sq = 0.0;

    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      sum_sq += Csqrmag(dlay(k));
      accum = Cadd(accum, Cmul(Conjg(afil(j)), dlay(k)));
    }

    error = Csub(ssig(i), accum);
    ssig(i) = error;

    scl2 = rate / (sum_sq + 1e-10);
	error = Cscl(Conjg(error),scl2);
    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      afil(j) =	Cadd(Cscl(afil(j), scl1),Cmul(error, dlay(k)));
    }

    dptr = bump(dptr);
  }
}

static void
lmsr_adapt_n(LMSR lms) {
  int i, j, k;
  REAL sum_sq, scl1, scl2;
  COMPLEX accum, error;
  scl1 = 1.0 - rate * leak;
  for (i = 0; i < ssiz; i++) {

    dlay(dptr) = ssig(i);
    accum = cxzero;
    sum_sq = 0.0;

    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      sum_sq += Csqrmag(dlay(k));
      accum = Cadd(accum, Cmul(Conjg(afil(j)), dlay(k)));
    }

    error = Csub(ssig(i), accum);
    ssig(i) = accum;

    scl2 = rate / (sum_sq + 1e-10);
	error = Cscl(Conjg(error),scl2);
    for (j = 0; j < asiz; j++) {
      k = wrap(j);
      afil(j) =	Cadd(Cscl(afil(j), scl1),Cmul(error, dlay(k)));
    }

    dptr = bump(dptr);
  }
}

#endif

extern void
lmsr_adapt(LMSR lms) {
  switch(lms->filter_type) {
  case LMADF_NOISE:
    lmsr_adapt_n(lms);
    break;        
  case LMADF_INTERFERENCE:
   lmsr_adapt_i(lms);
    break;
  }
}  
