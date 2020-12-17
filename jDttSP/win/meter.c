#include <common.h>
#ifdef _WINDOWS

DttSP_EXP float
calculate_meters(METERTYPE mt) {
  float result;
  REAL tmp;
  uni.meter.type = mt;
  if (getChan_nowait(uni.meter.chan.c, (char *) &tmp, sizeof(REAL)) != 0)
    uni.meter.val = tmp;
  result = (float) uni.meter.val;
  return result;
}

#endif
