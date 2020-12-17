#ifndef _power_spectrum_h
#define _power_spectrum_h

#include <fftw.h>
#include <complex.h>
#include <bufvec.h>

#ifndef _PWSMODE
#define _PWSMODE

typedef enum { MAG, DB } SPECTRUMTYPE;

typedef enum _powerspectrummode {
  POSTFILTER, PREFILTER, AUDIO 
} PWSMODE;

typedef enum _pws_submode {
  OFF, SPECTRUM, PANADAPTER, SCOPE, PHASE, PHASE2, WATERFALL, HISTOGRAM
} PWSSUBMODE;

typedef struct _powerspectrum {
  struct {
    int size;
    fftw_plan plan;
  } fft;
  RLB WindowBuf, WindowBuf2, results;
  CXB WindowedDataBuf, PowerSpectrumFFTresults;
  struct {
    RLB win, res;
    CXB wdat, zdat;
  } buf;
  struct {
    PWSMODE main;
    PWSSUBMODE sub;
    SPECTRUMTYPE spec;
  } flav;
} *PWS, powerspectrum;

typedef struct _powerspectrum {
  fftw_plan pwrspecfft;
  int fftsize;
  RLB WindowBuf, WindowBuf2, results;
  CXB WindowedDataBuf, PowerSpectrumFFTresults;
  PWSMODE Mode;
  PWSSUBMODE SubMode;
  SPECTRUMTYPE SpectrumType;
} *PWS, powerspectrum;

extern PWS newPowerSpectrum(int size, SPECTRUMTYPE SpectrumType);
extern void delPowerSpectrum(PWS pws);
extern void process_powerspectrum(float *results, int numpoints);

#endif
