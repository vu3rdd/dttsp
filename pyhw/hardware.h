/* hardware.h */

#ifndef _HARDWARE_H
#define _HARDWARE_H

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>  
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/socket.h>  
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <setjmp.h>

#include <stdlib.h>
#include <values.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <libgen.h>

#include <pthread.h>
#include <semaphore.h>

#include <linux/ppdev.h>
#include <linux/parport.h>

#ifndef PRIVATE
#define PRIVATE static
#endif

typedef int BOOLEAN;
typedef unsigned char BYTE;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

// ======================================================
// Constants and PRIVATE Variables
// ======================================================

typedef enum _bandplan
{
  IARU1 = 1,
  IARU2 = 2,
  IARU3 = 3,
} BandPlan;

//Constants for BPF relays

typedef enum _bandsetting
{
  bsnone = 0,			//none
  bs0 = 0x01,			//2.5MHz LPF
  bs1 = 0x02,			//2-6MHz BPF
  bs2 = 0x08,			//5-12MHz BPF
  bs3 = 0x04,			//10-24MHz BPF
  bs4 = 0x10,			//20-40MHz BPF
  bs5 = 0x20,			//35-60MHz BPF
} BandSetting;

//PIO Control Pin Numbers

typedef enum _ctrlpin
{
  EXT = 1,			//C0
  BPF = 14,			//C1
  DAT = 16,			//C2
  ADR = 17,			//C3
} CtrlPin;

//PIO Status Mask

typedef enum _statuspin
{
  PIN_12 = 0x08,		//S3
  Dash = 0x10,			//S4
  Dot = 0x20,			//S5
  PTT = 0x40,			//S6
  PIN_11 = 0x80,		//S7
} StatusPin;

//External control port mask

typedef enum _extpin
{
  P1 = 0x01,
  P2 = 0x02,
  P3 = 0x04,
  P4 = 0x08,
  P5 = 0x10,
  P6 = 0x20,
  P7 = 0x40,
} ExtPin;

//BEGIN RFE CONTROLS =====================================================

//Control and data lines for RFE serial decoders
#define SER  1
#define SCK  2
#define SCLR_NOT  4
#define DCDR_NE  32

//RFE 1:4 Decoder (74HC139) values to drive shift register RCK lines
typedef enum _rfe_rck
{
  IC11 = 0,
  IC7 = 8,
  IC9 = 24,
  IC10 = 16,
} RFE_RCK;

//RFE control constants
#define LPF0  1			//On board low pass filter relays
#define LPF1 2
#define LPF2  4
#define LPF3 8
#define LPF4 16
#define LPF5  32
#define LPF6  64
#define LPF7  128
#define LPF8  1
#define LPF9 2

#define BPF0  128		//Band pass filter relays
#define BPF1  64
#define BPF2  16		//Note: BPF2 and BPF3 are reverse order
#define BPF3  32		//on BPF board
#define BPF4  8
#define BPF5  4

#define PAF0  1			//Power Amplifier low pass filters
#define PAF1  2
#define PAF2  4
#define PAF3  8
#define PAF4  16
#define PAF5  32
#define PAFR  64		//Power amplifier TR relay

#define ATUCTL  128		//Automatic Tuning Unit control

#define AMP_RLYS  3		//Controls both AMP1 and AMP2 relays
#define XVTR_RLY  8		//Switches 2M transverter switching relayinto signal path
#define ATTN_RLY  16		//Attenuator relay
#define XVTR_TR_RLY  4		//2M transverter TR relay (on for RX) XVRX on schematic
#define IMPULSE_RLY  32		//Impulse circuit switching relay
#define SPARE_CTRL  64		//Spare control line to PA


//Constants latch outputs

#define TR  0x40
#define MUTE  0x80
#define GAIN  0x80
#define WRB  0x40
#define RESET  0x80

//DDS Control Constants
#define COMP_PD  0x10		//DDS Comparator power down
#define DIG_PD  0x01		//DDS Digital Power down
#define BYPASS_PLL  0x20	//Bypass DDS PLL
#define INT_IOUD  0x01		//Internal IO Update
#define OSK_EN  0x20		//Offset Shift Keying enable
#define OSK_INT  0x10		//Offset Shift Keying
#define BYPASS_SINC  0x40	//Bypass Inverse Sinc Filter
#define PLL_RANGE  0x40		//Set PLL Range

typedef struct _rig
{
  //PIO register base address
//  unsigned short baseAdr;
  double min_freq;		// minimum allowable tuning frequency
  double max_freq;		// maximum allowable tuning frequency
  unsigned radio_number;
  unsigned short Adr;
  int m_IC7_Memory;
  //RFE relay memory
  BOOLEAN rfe_enabled;		//True if RFE board is enabled
  BOOLEAN xvtr_enabled;		//Transverter is enabled
  BOOLEAN extended;
  BOOLEAN xvtr_tr_logic;

  // END RFE controls
  //Latch Memories

  BandSetting band_relay;
  int external_output;
  int mute_relay;
  int transmit_relay;
  int gain_relay;
  int latch_delay;

  //DDS Clock properties
  double dds_clock;
  int pll_mult;
  double dds_clock_correction;
  double sysClock;
  int ioud_clock;
  unsigned short dac_mult;

  //DDS Frequency Control properties
  double dds_freq;
  double if_freq;
  BOOLEAN if_shift;
  BOOLEAN spur_reduction;
  double dds_step_size;
  int sample_rate;
  int fft_length;
  double FFT_Bin_Size;
  int tune_fft;
  double tune_frac_rel;
  double vfo_offset;
  double last_VFO;

  //Current Bandplan
  BandPlan curBandPlan;

  double TWO_TO_THE_48_DIVIDED_BY_200;
  long last_tuning_word;

  int fd;
  BOOLEAN needs_OSC_change;
  double OSC_change;
} Rig;

extern Rig myRig;

extern BOOLEAN openRig(char *port);
extern void closeRig(void);

extern BOOLEAN getExtended(void);
extern void setExtended(BOOLEAN value);
extern BOOLEAN getRFE_Enabled(void);
extern void setRFE_Enabled(BOOLEAN value);
extern BOOLEAN getXVTR_Enabled(void);
extern void setXVTR_Enabled(BOOLEAN value);
extern BOOLEAN getXVTR_TR_Logic(void);
extern void setXVTR_TR_Logic(BOOLEAN value);
extern int getLatchDelay(void);
extern void setLatchDelay(int value);
extern double getMinFreq(void);
extern double getMaxFreq(void);
//extern unsigned short getBaseAddr(void);
//extern void setBaseAddr(unsigned short value);
extern BandSetting getBandRelay(void);
extern void setBandRelay(BandSetting value);
extern BOOLEAN getTransmitRelay(void);
extern void setTransmitRelay(BOOLEAN value);
extern BOOLEAN getMuteRelay(void);
extern void setMuteRelay(BOOLEAN value);
extern BOOLEAN getGainRelay(void);
extern void setGainRelay(BOOLEAN value);
extern int getExternalOutput(void);
extern void setExternalOutput(int value);
extern double getDDSClockCorrection(void);
extern void setDDSClockCorrection(double value);
extern int getPLLMult(void);
extern void setPLLMult(int value);
extern double getDDSClock(void);
extern void setDDSClock(double value);
extern BOOLEAN getIFShift(void);
extern void setIFShift(BOOLEAN value);
extern BOOLEAN getSpurReduction(void);
extern void setSpurReduction(BOOLEAN value);
extern double getIFFreq(void);
extern void setIFFreq(double value);
extern double getDDSFreq(void);
extern void setDDSFreq(double value);
extern int getSampleRate(void);
extern void setSampleRate(int value);
extern int getFFTLength(void);
extern void setFFTLength(int value);
extern int getTuneFFT(void);
extern double getTuneFracRel(void);
extern double getVFOOffset(void);
extern void setVFOOffset(double value);
extern int getIOUDClock(void);
extern void setIOUDClock(int value);
extern unsigned short getDACMult(void);
extern void setDACMult(unsigned short value);
extern BOOLEAN InputPin(StatusPin vStatusPin);
extern BYTE StatusPort(void);
extern void RigInit(void);
extern void PowerOn(void);
extern void StandBy(void);
extern void SetExt(ExtPin pin);
extern void ResExt(ExtPin pin);
extern BOOLEAN PinValue(ExtPin pin);
extern void SetBPF(double VFOValue);
extern BOOLEAN IsHamBand(BandPlan b);
extern void TestPort(void);
extern void RCKStrobe(BOOLEAN ClearReg, RFE_RCK Reg);
extern void SRLoad(RFE_RCK Reg, int Data);
extern void ResetRFE(void);
extern BOOLEAN getAMP_Relay(void);
extern void setAMP_Relay(BOOLEAN value);
extern BOOLEAN getATTN_Relay(void);
extern void setATTN_Relay(BOOLEAN value);
extern BOOLEAN getXVTR_TR_Relay(void);
extern void setXVTR_TR_Relay(BOOLEAN value);
extern BOOLEAN getXVTR_Relay(void);
extern void setXVTR_Relay(BOOLEAN value);
extern BOOLEAN getIMPULSE_Relay(void);
extern void setIMPULSE_Relay(BOOLEAN);
extern void Impulse(void);
#endif
