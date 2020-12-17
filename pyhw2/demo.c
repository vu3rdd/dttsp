// demo.c

#include <hardware.h>

#define FALSE 0
#define TRUE 1

int
main(int argc, char **argv) {
#if 0
  if (!openPort("/dev/parport0"))
    perror("/dev/parport0"), exit(1);
#endif
  if (!openPort("/dev/null"))
    perror("/dev/null"), exit(1);

  setRFE_Enabled(TRUE);
  ResetRFE();
  setXVTR_Enabled(FALSE);
  setExtended(FALSE);
  setXVTR_TR_Relay(FALSE);
  setBandRelay(bs0);
  Init();
  setVFOOffset(0.0);
  setATTN_Relay(TRUE);
  setTransmitRelay(FALSE);
  setGainRelay(TRUE);
  setSpurReduction(FALSE);
  setExternalOutput(FALSE);
  setDDSClockCorrection(0.0);
  setDDSFreq(13.845);
  setMuteRelay(FALSE);

  fprintf(stderr, "Ready\n");
  {
    int i;
    for (i = 0; i < 100; i++)
      setDDSFreq(14.060);
  }
  fprintf(stderr, "Done\n");

  closePort();
  exit(0);
}
