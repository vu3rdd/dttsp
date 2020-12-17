#include <common.h>
 
#ifdef _WINDOWS

PWS
newPowerSpectrum(int size, SPECTRUMTYPE type) {
  PWS p = (PWS) safealloc(1, sizeof(powerspectrum), "New Power Spectrum");

  p->buf.win = newRLB(size, NULL, "Power Spectrum Window Buffer");
  p->buf.res = newRLB(32768, NULL, "Save buffer");
  p->buf.wdat = newCXB(size, NULL, "Power Spectrum Windowed Signal buffer");
  p->buf.zdat = newCXB(size, NULL, "Power Spectrum FFT Results Buffer");
  p->fft.size = size;
  p->flav.spec = type;

  makewindow(BLACKMANHARRIS_WINDOW, size, RLBbase(p->buf.win));

#ifdef notdef  
  {
    REAL *tmp = (REAL *) alloca(size * sizeof(REAL));
    int n = size / 2, nb = n * sizeof(REAL);
    memcpy(&tmp[n], RLBbase(p->buf.win), nb);
    memcpy(tmp, &RLBdata(p->buf.win, n), nb);
  }
#endif

  p->fft.plan = fftw_create_plan(size, FFTW_FORWARD, uni.wisdom.bits);

  return p;
}

void
delPowerSpectrum(PWS pws) {
  delCXB(pws->buf.wdat);
  delCXB(pws->buf.zdat);
  delRLB(pws->buf.win);
  delRLB(pws->buf.win2);
  fftw_destroy_plan(pws->fft.plan);
  safefree((char *) pws);
}

DttSP_EXP BOOLEAN
process_spectrum(float *results) {
  int i, j, n = CXBsize(uni.powsp.buf);

  if (getChan_nowait(uni.powsp.chan.c,
		     (char *) CXBbase(uni.powsp.buf),
		     uni.powsp.fft.size * sizeof(COMPLEX))
      == TRUE) {
    
    // Window the data to minimize "splatter" in the powerspectrum
    for (i = 0; i < uni.powsp.fft.size; i++)
      CXBdata(uni.powsp.gen->buf.wdat, i) = Cscl(CXBdata(uni.powsp.buf, i),
						 RLBdata(uni.powsp.gen->buf.win, i));
    
    // Compute the FFT
    fftw_one(uni.powsp.gen->fft.plan,
	     (fftw_complex *) CXBbase(uni.powsp.gen->buf.wdat),
	     (fftw_complex *) CXBbase(uni.powsp.gen->buf.zdat));
    for (i = 0; i < n / 2; i++) {
      RLBdata(uni.powsp.gen->buf.res, 8 * i) =
	results[8 * i] = 
	(float) (10.0 *
		 log10(1e-160 +
		       Csqrmag(CXBdata(uni.powsp.gen->buf.zdat,
				       (i + uni.buflen)))));
      RLBdata(uni.powsp.gen->buf.res, 8 * (i + uni.buflen)) =
	results[8 * (i + uni.buflen)] =
	(float) (10.0 *
		  log10(1e-160 +
			Csqrmag(CXBdata(uni.powsp.gen->buf.zdat, i))));
    } 

    // Interpolate for display on high resolution monitors

    for (i = 0; i < 32768; i += 8)
      for (j = 1; j < 8; j++)
	RLBdata(uni.powsp.gen->buf.res, i + j) =
	  results[i + j] = (float) 0.125 * (results[i] * (float) (8 - j)
					    + results[i + 8] * (float) j);
    return TRUE;
  } else {
    for (i = 0; i < 32768; i++)
      results[i] = (float) RLBdata(uni.powsp.gen->buf.res, i);
    return FALSE;
  }
}

DttSP_EXP BOOLEAN
process_phase(float *results, int numpoints) {
  int i, j;
  if (getChan_nowait(uni.powsp.chan.c,
		     (char *) CXBbase(uni.powsp.buf),
		     uni.powsp.fft.size * sizeof(COMPLEX))
      == TRUE) {
    for (i = 0, j = 0; i < numpoints; i++, j += 2) {
      RLBdata(uni.powsp.gen->buf.res, j) =
	results[j] = (float) CXBreal(uni.powsp.buf, i);
      RLBdata(uni.powsp.gen->buf.res, j + 1) =
	results[j + 1] = (float) CXBimag(uni.powsp.buf, i);
    } return TRUE;
  } else {
    for (i = 0; i < 2 * numpoints; i++)
      results[i] = (float) RLBdata(uni.powsp.gen->buf.res, i);
    return FALSE;
  }
}

// Send out Real Signal, reals only

DttSP_EXP BOOLEAN
process_scope(float *results) {
  int i;
  if (getChan_nowait(uni.powsp.chan.c,
		     (char *) CXBbase(uni.powsp.buf),
		     uni.powsp.fft.size * sizeof(COMPLEX))
      == TRUE) {
    for (i = 0; i < uni.powsp.fft.size; i++) {
      results[i] = (float) CXBreal(uni.powsp.buf, i);
    } return TRUE;
  } else {
    for (i = 0; i < uni.powsp.fft.size; i++)
      results[i] = (float) CXBreal(uni.powsp.buf, i);
    return FALSE;
  }
}

#endif
