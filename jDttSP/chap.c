/* chap.c 

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

#include <chap.h>

REAL
ChAp_eval(ChAp ca, REAL x, BOOLEAN *err) {
  int i;
  REAL d, e, u, v;

  if (x < ChApLob(ca)) {
    x = ChApLob(ca);
    if (err) *err = TRUE;
  } else if (x > ChApHib(ca)) {
    x = ChApHib(ca);
    if (err) *err = TRUE;
  } else if (err) *err = FALSE;

  d = e = 0.0;
  u = (2.0 * x - ChApDif(ca)) / ChApDif(ca);
  v = 2.0 * u;
  for (i = ChApLen(ca) - 1; i > 0; --i) {
    REAL sv = d;
    d = v * d - e + ChApCoef(ca, i);
    e = sv;
  }

  return u * d - e + 0.5 * ChApCoef(ca, 0);
}

ChAp
ChAp_fit(ChAp ca) {
  int i, j, n = ChApLen(ca);
  REAL (*func)(REAL) = ChApFunc(ca),
       bma = 0.5 * (ChApHib(ca) - ChApLob(ca)),
       bpa = 0.5 * (ChApHib(ca) + ChApLob(ca)),
       fac = 2.0 / n,
      *tbl = newvec_REAL(n, ChApTag(ca));
  for (j = 0; j < n; j++) {
    REAL y = cos(M_PI * (j + 0.5) / n);
    tbl[j] = (*func)(y * bma + bpa);
  }
  for (i = 0; i < n; i++) {
    REAL sum = 0.0;
    for (j = 0; j < n; j++)
      sum += tbl[j] * cos((M_PI * i) * ((j + 0.5) / n));
    ChApCoef(ca, i) = fac * sum;
  }
  delvec_REAL(tbl);
  return ca;
}

ChAp
newChAp(REAL (*func)(REAL arg),
	REAL lo,
	REAL hi,
	int len,
	char *tag) {
  ChAp ca = (ChAp) safealloc(1, sizeof(ChApDesc), tag);
  ChApFunc(ca) = func;
  ChApLen(ca) = len;
  ChApLob(ca) = lo;
  ChApHib(ca) = hi;
  ChApDif(ca) = hi - lo;
  ChApCoefBase(ca) = newvec_REAL(len, tag);
  ChApTag(ca) = strdup(tag);
  return ChAp_fit(ca);
}

void
delChAp(ChAp ca) {
  if (ca) {
    safefree((char *) ChApCoefBase(ca));
    safefree(ChApTag(ca));
    safefree((char *) ca);
  }
}


