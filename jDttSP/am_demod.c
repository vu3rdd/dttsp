/* am_demod.c */

#include <am_demod.h>
#include <cxops.h>

/*------------------------------------------------------------------------------*/
/* private to AM */
/*------------------------------------------------------------------------------*/

static void
init_pll(AMD am,
	 REAL samprate,
	 REAL freq,
	 REAL lofreq, 
	 REAL hifreq,
	 REAL bandwidth) {
  REAL fac = TWOPI / samprate;
  am->pll.freq.f = freq * fac;
  am->pll.freq.l = lofreq * fac;
  am->pll.freq.h = hifreq * fac;
  am->pll.phs = 0.0;
  am->pll.delay = cxJ;

  am->pll.iir.alpha = bandwidth * fac; /* arm filter */
  am->pll.alpha = am->pll.iir.alpha * 0.3;   /* pll bandwidth */
  am->pll.beta = am->pll.alpha * am->pll.alpha * 0.25; /* second order term */
  am->pll.fast_alpha = am->pll.alpha;
}

static void
pll(AMD am, COMPLEX sig) {
  COMPLEX z = Cmplx(cos(am->pll.phs), sin(am->pll.phs));
  REAL diff;

  am->pll.delay.re =  z.re * sig.re + z.im * sig.im;
  am->pll.delay.im = -z.im * sig.re + z.re * sig.im;
  diff = Cmag(sig) * ATAN2(am->pll.delay.im, am->pll.delay.re);

  am->pll.freq.f += am->pll.beta * diff;

  if (am->pll.freq.f < am->pll.freq.l)
    am->pll.freq.f = am->pll.freq.l;
  if (am->pll.freq.f > am->pll.freq.h)
    am->pll.freq.f = am->pll.freq.h;

  am->pll.phs += am->pll.freq.f + am->pll.alpha * diff;

  while (am->pll.phs >= TWOPI) am->pll.phs -= TWOPI;
  while (am->pll.phs < 0) am->pll.phs += TWOPI;
}

static double
dem(AMD am) {
  am->lock.curr = 0.999 * am->lock.curr + 0.001 * fabs(am->pll.delay.im);
  
  /* env collapse? */
 /* if ((am->lock.curr < 0.05) && (am->lock.prev >= 0.05)) {
    am->pll.alpha = 0.1 * am->pll.fast_alpha;
    am->pll.beta = am->pll.alpha * am->pll.alpha * 0.25;
  } else if ((am->pll.alpha > 0.05) && (am->lock.prev <= 0.05)) {
	  am->pll.alpha = am->pll.fast_alpha;
  }*/
  am->lock.prev = am->lock.curr;
  am->dc = 0.99*am->dc + 0.01*am->pll.delay.re;
  return am->pll.delay.re-am->dc;
}

/*------------------------------------------------------------------------------*/
/* public */
/*------------------------------------------------------------------------------*/

void
AMDemod(AMD am) {
  int i;
  double demout;
  switch (am->mode) {
  case SAMdet:
    for (i = 0; i < am->size; i++) {
      pll(am, CXBdata(am->ibuf, i));
      demout = dem(am);
      CXBdata(am->obuf, i) = Cmplx(demout, demout);
    }
    break;
  case AMdet:
    for (i = 0; i < am->size; i++) {
      am->lock.curr = Cmag(CXBdata(am->ibuf, i));
      am->dc = 0.9999 * am->dc + 0.0001 * am->lock.curr;
      am->smooth = 0.5 * am->smooth + 0.5 * (am->lock.curr - am->dc);
      /* demout = am->smooth; */
      CXBdata(am->obuf, i) = Cmplx(am->smooth, am->smooth);
    }
    break;
  }
}

AMD
newAMD(REAL samprate,
       REAL f_initial,
       REAL f_lobound,
       REAL f_hibound,
       REAL f_bandwid,
       int size,
       COMPLEX *ivec,
       COMPLEX *ovec,
       AMMode mode,
       char *tag) {
  AMD am = (AMD) safealloc(1, sizeof(AMDDesc), tag);

  am->size = size;
  am->ibuf = newCXB(size, ivec, tag);
  am->obuf = newCXB(size, ovec, tag);
  am->mode = mode;
  init_pll(am,
	   samprate,
	   f_initial,
	   f_lobound,
	   f_hibound,
	   f_bandwid);

  am->lock.curr = 0.5;
  am->lock.prev = 1.0;
  am->dc = 0.0;
  
  return am;
}

void
delAMD(AMD am) {
  if (am) {
    delCXB(am->ibuf);
    delCXB(am->obuf);
    safefree((char *) am);
  }
}
