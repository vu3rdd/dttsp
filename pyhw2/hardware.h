// hardware.h
// version 2

#ifndef _hardware_h
#define _hardware_h

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
#ifndef PUBLIC
#define PUBLIC extern
#endif

typedef int BOOL;
typedef unsigned char BYTE;

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

PUBLIC double DttSP_SampleRate;

typedef enum {
  IARU1 = 1,
  IARU2 = 2,
  IARU3 = 3,
} BandPlan;

typedef enum {
  BYPASS,
  MEMORY,
  FULL,
  LAST,
} ATUTuneMode;

typedef enum {
  POSITIVE = 0,	// DEMI144-28FRS
  NEGATIVE,	// 25W DEMI
  NONE,		// For Elecraft or similar XVTR
} XVTRTRMode;

// Constants for BPF relays
typedef enum {
  bsnone = 0,			//none
  bs0 = 0x01,			//2.5MHz LPF
  bs1 = 0x02,			//2-6MHz BPF
  bs2 = 0x08,			//5-12MHz BPF
  bs3 = 0x04,			//10-24MHz BPF
  bs4 = 0x10,			//20-40MHz BPF
  bs5 = 0x20,			//35-60MHz BPF
} BandSetting;

// PIO Control Pin Numbers
typedef enum {
  EXT = 1,			//C0
  BPF = 14,			//C1
  DAT = 16,			//C2
  ADR = 17,			//C3
} CtrlPin;

// PIO Status Mask
typedef enum {
  PIN_12	= 0x08,		//S3
  Dash		= 0x10,		//S4
  Dot		= 0x20,		//S5
  PA_DATA	= 0x40,		//S6
  PIN_11	= 0x80,		//S7
} StatusPin;

// External control port mask
typedef enum {
  P1 = 0x01,
  P2 = 0x02,
  P3 = 0x04,
  P4 = 0x08,
  P5 = 0x10,
  P6 = 0x20,
  P7 = 0x40,
} ExtPin;

// RFE CONTROLS:

// Control and data lines for RFE serial decoders
#define SER		0x01
#define SCK		0x02
#define SCLR_NOT	0x04
#define DCDR_NE		0x20

// RFE 1:4 Decoder (74HC139) values to drive shift register RCK lines
typedef enum {
  IC11 = 0x00,
  IC7  = 0x08,
  IC9  = 0x18,
  IC10 = 0x10,
} RFE_RCK;

// RFE control constants

// On board low pass filter relays
#define LPF0 0x01
#define LPF1 0x02
#define LPF2 0x04
#define LPF3 0x08
#define LPF4 0x10
#define LPF5 0x20
#define LPF6 0x40
#define LPF7 0x80
#define LPF8 0x01
#define LPF9 0x02

// Band pass filter relays
#define BPF0 0x80
#define BPF1 0x40
// Note: BPF2 and BPF3 are reverse order
// on BPF board
#define BPF2 0x10
#define BPF3 0x20
#define BPF4 0x08
#define BPF5 0x04

// IC11 Variables
// Register memory for IC11
PUBLIC int ic11_memory;		
// Power Amplifier low pass filters
#define PAF0		0x01
#define PAF1		0x02				
#define PAF2		0x04
// Analog to Digital Clock
#define ADC_CLK		0x08
// Analog to Digital Data In
#define ADC_DI		0x10
// Analog to Digital Chip Select (inverse logic)
#define ADC_CS_NOT	0x20
// Power amplifier TR relay
#define PATR		0x40
// Automatic Tuning Unit control
#define ATUCTL		0x80

// IC7 Variables
// Register memory for IC7
PUBLIC int ic7_memory;		
// Controls both AMP1 and AMP2 relays
#define AMP_RLYS	0x03	
// 2M transverter TR relay (on for RX) XVRX on schematic
#define XVTR_TR_RLY	0x04	
// Switches 2M transverter switching relay into signal path
#define XVTR_RLY	0x08	
// Attenuator relay
#define ATTN_RLY	0x10	
// Impulse circuit switches impulse into signal path
#define IMPULSE_RLY	0x20	
// Spare control line to PA (inverse logic)
#define PA_BIAS_NOT	0x40	
// Generates 5ns Pulse
#define IMPULSE		0x80	

PUBLIC BOOL rfe_enabled;		// True if RFE board is enabled
PUBLIC BOOL xvtr_enabled;		// Transverter is enabled

// done with RFE controls

// PA variables
PUBLIC BOOL pa_enabled;
// 0
#define PA_LPF_OFF	0				
// 1
#define PA_LPF_12_10	PAF0				
// 2
#define PA_LPF_17_15	PAF1				
// 3
#define PA_LPF_30_20	PAF0+PAF1		
// 4
#define PA_LPF_60_40	PAF2				
// 5
#define PA_LPF_80	PAF2+PAF0		
// 6
#define PA_LPF_160	PAF2+PAF1		
#define PA_FORWARD_PWR	0
#define PA_REVERSE_PWR	1

// Constants latch outputs
#define  TR	0x40
#define  MUTE	0x80
#define  GAIN	0x80
#define  WRB	0x40
#define  RESET	0x80

// Latch Memories
PUBLIC BandSetting band_relay;
PUBLIC int external_output;
PUBLIC int mute_relay;
PUBLIC int transmit_relay;
PUBLIC int gain_relay;
PUBLIC int latch_delay;

// DDS Control Constants
// DDS Comparator power down
#define COMP_PD		0x10		
// DDS Digital Power down
#define DIG_PD		0x01		
// Bypass DDS PLL
#define BYPASS_PLL	0x20		
// Internal IO Update
#define INT_IOUD	0x01		
// Offset Shift Keying enable
#define OSK_EN		0x20		
// Offset Shift Keying
#define OSK_INT		0x10		
// Bypass Inverse Sinc Filter
#define BYPASS_SINC	0x40		
// Set PLL Range
#define PLL_RANGE	0x40		

// DDS Clock properties
PUBLIC double dds_clock;
PUBLIC int pll_mult;
PUBLIC double dds_clock_correction;
PUBLIC double sysClock;
PUBLIC int ioud_clock;
PUBLIC ushort dac_mult;

// DDS Frequency Control properties
PUBLIC double dds_freq;
PUBLIC double if_freq;
PUBLIC BOOL if_shift;
PUBLIC BOOL spur_reduction;
PUBLIC double dds_step_size;
PUBLIC int sample_rate;
PUBLIC int fft_length;
PUBLIC double FFT_Bin_Size;
PUBLIC int tune_fft;
PUBLIC double tune_frac_rel;
PUBLIC double vfo_offset;
//extern double last_VFO;

PUBLIC double min_freq;
PUBLIC double max_freq;

// PIO register base address
PUBLIC u_short baseAdr;

// Current Bandplan
PUBLIC BandPlan curBandPlan;

PUBLIC double TWO_TO_THE_48_DIVIDED_BY_200;
PUBLIC long last_tuning_word;
// private Mutex parallel_mutex;
// private Mutex dataline_mutex;

PUBLIC BOOL usb_enabled;

#define SDR1K_LATCH_EXT 0x01
#define SDR1K_LATCH_BPF 0x02

//------------------------------------------------------------------------

PUBLIC BOOL        openPort(char *port);
PUBLIC void        closePort(void);
PUBLIC void        USB_Sdr1kLatch(int, BYTE);
PUBLIC BYTE	   USB_Sdr1kGetStatusPort(void);
PUBLIC int	   USB_Sdr1kGetADC(void);
PUBLIC void        USB_Sdr1kDDSReset(void);
PUBLIC void        USB_Sdr1kDDSWrite(BYTE addr, BYTE data);
PUBLIC void        USB_Sdr1kSRLoad(BYTE reg, BYTE data);
PUBLIC void        DttSP_ChangeOsc(double val);

PUBLIC void        Init                  (void);
PUBLIC void        PowerOn               (void);
PUBLIC void        StandBy               (void);
PUBLIC void        Impulse               (void);

PUBLIC BYTE        StatusPort            (void);
PUBLIC void        SetExt                (ExtPin pin);
PUBLIC void        ResExt                (ExtPin pin);
PUBLIC BOOL        PinValue              (ExtPin pin);
PUBLIC void        SetBPF                (double vfo_value);
PUBLIC void        TestPort              (void);
PUBLIC void        RCKStrobe             (BOOL ClearReg, RFE_RCK Reg);
PUBLIC void        SRLoad                (RFE_RCK Reg, int Data);
PUBLIC void        ResetRFE              (void);
PUBLIC void        PA_SetLPF             (int i);
PUBLIC BYTE        PA_GetADC             (int chan);
PUBLIC BOOL        PA_ATUTune            (ATUTuneMode mode);

PUBLIC BOOL        getEnableLPF0         (void);
PUBLIC void        setEnableLPF0         (BOOL value);
PUBLIC BOOL        getExtended           (void);
PUBLIC void        setExtended           (BOOL value);
PUBLIC BOOL        getX2Enabled          (void);
PUBLIC void        setX2Enabled          (BOOL value);
PUBLIC int         getX2Delay            (void);
PUBLIC void        setX2Delay            (int value);
PUBLIC BOOL        getRFE_Enabled        (void);
PUBLIC void        setRFE_Enabled        (BOOL value);
PUBLIC BOOL        getPA_Enabled         (void);
PUBLIC void        setPA_Enabled         (BOOL value);
PUBLIC BOOL        getXVTR_Enabled       (void);
PUBLIC BOOL        setXVTR_Enabled       (BOOL value);
PUBLIC BOOL        getUSB_Enabled        (void);
PUBLIC void        setUSB_Enabled        (BOOL value);
PUBLIC XVTRTRMode  getCurrentXVTRTRMode  (void);
PUBLIC void        setCurrentXVTRTRMode  (XVTRTRMode value);
PUBLIC int         getLatchDelay         (void);
PUBLIC void        setLatchDelay         (int value);
PUBLIC double      getMinFreq            (void);
PUBLIC double      getMaxFreq            (void);
PUBLIC u_short     getBaseAddr           (void);
PUBLIC u_short     setBaseAddr           (u_short value);
PUBLIC BandSetting getBandRelay          (void);
PUBLIC void        setBandRelay          (BandSetting value);
PUBLIC BOOL        getTransmitRelay      (void);
PUBLIC void        setTransmitRelay      (BOOL value);
PUBLIC BOOL        getMuteRelay          (void);
PUBLIC void        setMuteRelay          (BOOL value);
PUBLIC BOOL        getGainRelay          (void);
PUBLIC void        setGainRelay          (BOOL value);
PUBLIC int         getExternalOutput     (void);
PUBLIC void        setExternalOutput     (int value);
PUBLIC double      getDDSClockCorrection (void);
PUBLIC void        setDDSClockCorrection (double value);
PUBLIC int         getPLLMult            (void);
PUBLIC void        setPLLMult            (int value);
PUBLIC double      getDDSClock           (void);
PUBLIC void        setDDSClock           (double value);
PUBLIC BOOL        getIFShift            (void);
PUBLIC void        setIFShift            (BOOL value);
PUBLIC BOOL        getSpurReduction      (void);
PUBLIC BOOL        setSpurReduction      (BOOL value);
PUBLIC double      getIFFreq             (void);
PUBLIC void        setIFFreq             (double value);
PUBLIC double      getDDSFreq            (void);
PUBLIC void        setDDSFreq            (double value);
PUBLIC int         getSampleRate         (void);
PUBLIC void        setSampleRate         (int value);
PUBLIC int         getFFTLength          (void);
PUBLIC void        setFFTLength          (int value);
PUBLIC int         getTuneFFT            (void);
PUBLIC double      getTuneFracRel        (void);
PUBLIC double      getVFOOffset          (void);
PUBLIC void        setVFOOffset          (double value);
PUBLIC int         getIOUDClock          (void);
PUBLIC void        setIOUDClock          (int value);
PUBLIC u_short     getDACMult            (void);
PUBLIC void        setDACMult            (u_short value);
PUBLIC BOOL        getAMP_Relay          (void);
PUBLIC BOOL        setAMP_Relay          (BOOL value);
PUBLIC BOOL        getATTN_Relay         (void);
PUBLIC void        setATTN_Relay         (BOOL value);
PUBLIC BOOL        getXVTR_TR_Relay      (void);
PUBLIC BOOL        setXVTR_TR_Relay      (BOOL value);
PUBLIC BOOL        getXVTR_Relay         (void);
PUBLIC BOOL        setXVTR_Relay         (BOOL value);
PUBLIC BOOL        getIMPULSE_Relay      (void);
PUBLIC BOOL        setIMPULSE_Relay      (BOOL value);
PUBLIC BOOL        getPA_TransmitRelay   (void);
PUBLIC BOOL        setPA_TransmitRelay   (BOOL value);
PUBLIC BOOL        getPA_BiasOn          (void);
PUBLIC BOOL        setPA_BiasOn          (BOOL value);

//------------------------------------------------------------------------

#endif
