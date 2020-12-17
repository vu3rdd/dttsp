/* keyd.c */
/*
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

//#include <linux/rtc.h>
#include <fromsys.h>
#include <banal.h>
#include <splitfields.h>
#include <datatypes.h>
#include <bufvec.h>
#include <cxops.h>
#include <ringb.h>
#include <chan.h>
#include <oscillator.h>
#include <cwtones.h>
#include <pthread.h>
#include <semaphore.h>
#include <keyer.h>

#define SAMP_RATE (48000)

// # times key is sampled per sec
// > 64 requires root on Linux
//#define RTC_RATE (128)
#define RTC_RATE (64)

// # samples generated during 1 clock tick at RTC_RATE
#define TONE_SIZE (SAMP_RATE / RTC_RATE)

// ring buffer size; > 1 sec at this sr
#define RING_SIZE (01 << 020)

KeyerState ks;
KeyerLogic kl;

static pthread_t play, key, update;
sem_t clock_fired, keyer_started, update_ok, poll_fired;

int fdser, fdrtc;
/*
jack_client_t *client;
jack_port_t *lport, *rport;
jack_ringbuffer_t *lring, *rring;
jack_nframes_t size; */
ringb_t *lring, *rring;
int size;

CWToneGen gen;
static BOOLEAN playing = FALSE, iambic = FALSE;
static double wpm = 18.0, freq = 750.0, ramp = 5.0, gain = 1.0;

//------------------------------------------------------------


DttSP_EXP void
CWtoneExchange(float *bufl,float*bufr,int nframes) {
	size_t bytesize = nframes*4;
	size_t numsamps;
	if ((numsamps = ringb_read_space(lring)) < bytesize) {
		memset(bufl,0,bytesize);
		memset(bufr,0,bytesize);
	} else {
		ringb_read(lring,(char *)bufl,bytesize);
		ringb_read(rring,(char *)bufr,bytesize);
	}
}

//------------------------------------------------------------

// generated tone -> output ringbuffer
void
send_tone(void) {
  if (ringb_write_space(lring) < TONE_SIZE * sizeof(float)) {
    //write(2, "overrun tone\n", 13);
    ringb_restart(lring, TONE_SIZE * sizeof(float));
    ringb_restart(rring, TONE_SIZE * sizeof(float));
  } else {
    int i;
    for (i = 0; i < gen->size; i++) {
      float l = (float)CXBreal(gen->buf, i),
	    r = (float)CXBimag(gen->buf, i);
      ringb_write(lring, (char *) &l, sizeof(float));
      ringb_write(rring, (char *) &r, sizeof(float));
    }
  }
}

// silence -> output ringbuffer
void
send_silence(void) {
  if (ringb_write_space(lring) < TONE_SIZE * sizeof(float)) {
    //write(2, "overrun zero\n", 13);
    ringb_restart(lring, TONE_SIZE * sizeof(float));
    ringb_restart(rring, TONE_SIZE * sizeof(float));
  } else {
    int i;
    for (i = 0; i < gen->size; i++) {
      float zero = 0.0;
      ringb_write(lring, (char *) &zero, sizeof(float));
      ringb_write(rring, (char *) &zero, sizeof(float));
    }
  }
}

//------------------------------------------------------------------------

// sound/silence generation
// tone turned on/off asynchronously

DttSP_EXP void
sound_thread_keyd(void) {
  for (;;) {
    sem_wait(&clock_fired);

    if (playing) {
      // CWTone keeps playing for awhile after it's turned off,
      // in order to allow for a decay envelope;
      // returns FALSE when it's actually done.
      playing = CWTone(gen);
      send_tone();
    } else {
      send_silence();
      // only let updates run when we've just generated silence
//      sem_post(&update_ok);
    }
  }

  pthread_exit(0);
}


BOOLEAN
read_key(double del, BOOLEAN dot, BOOLEAN dash) {
	extern BOOLEAN read_straight_key(KeyerState ks, BOOLEAN keyed);
	extern BOOLEAN read_iambic_key(KeyerState ks, BOOLEAN dot, BOOLEAN dash, KeyerLogic kl, double ticklen);

  if (iambic)
    return read_iambic_key(ks, dot, dash, kl, del);
  else
    return read_straight_key(ks, dot^dash);
}

/// Main keyer function,  called by a thread in the C#
DttSP_EXP void
key_thread(double del, BOOLEAN dash, BOOLEAN dot) {
	BOOLEAN keydown;

    // called after next tick and passed the
	// delay waitsince last one

    // read key; tell keyer elapsed time since last call
    keydown = read_key(del,dot,dash);


    if (!playing && keydown)
      CWToneOn(gen), playing = TRUE;
    else if (playing && !keydown)
      CWToneOff(gen);

    sem_post(&clock_fired);
}

//------------------------------------------------------------------------

// update keyer parameters via text input from stdin
// <wpm xxx> -> set keyer speed to xxx
// <gain xxx> -> set gain to xxx (dB)
// <freq xxx> -> set freq to xxx
// <ramp xxx> -> set attack/decay times to xxx ms


#define MAX_ESC (512)
#define ESC_L '<'
#define ESC_R '>'

void
updater(void) {
  for (;;) {
    int c;

    // get or wait for next input char
    if ((c = getchar()) == EOF) goto finish;

    // if we see the beginning of a command,
    if (c == ESC_L) {
      int i = 0;
      char buf[MAX_ESC];

      // gather up the remainder
      while ((c = getchar()) != EOF) {
	if (c == ESC_R) break;
	buf[i] = c;
	if (++i >= (MAX_ESC - 1)) break;
      }
      if (c == EOF) goto finish;
      buf[i] = 0;

      // wait until changes are safe
      sem_wait(&update_ok);

      if (!strncmp(buf, "wpm", 3))
	ks->wpm = wpm = atof(buf + 3);
      else if (!strncmp(buf, "ramp", 4)) {
	ramp = atof(buf + 4);
	setCWToneGenVals(gen, gain, freq, ramp, ramp);
      } else if (!strncmp(buf, "freq", 4)) {
	freq = atof(buf + 4);
	setCWToneGenVals(gen, gain, freq, ramp, ramp);
      } else if (!strncmp(buf, "gain", 4)) {
	gain = atof(buf + 4);
	setCWToneGenVals(gen, gain, freq, ramp, ramp);
      } else if (!strncmp(buf, "quit", 4))
	goto finish;

    } // otherwise go around again
  }

  // we saw an EOF or quit; kill other threads and exit neatly

 finish:
  pthread_cancel(play);
  pthread_cancel(key);
  pthread_exit(0);
}
DttSP_EXP void
updateKeyer(double nfreq, BOOLEAN niambic, double ngain, double nramp, double nwpm,
			BOOLEAN revpdl, int weight, double SampleRate) {
	ks->flag.iambic = niambic;
	iambic = niambic;
	ks->flag.revpdl = revpdl;
	ks->weight = weight;
	wpm = nwpm;
	gain = ngain;
	ramp = nramp;
	freq = nfreq;
	gen->osc.freq = 2.0 * M_PI * freq / SampleRate;
}
DttSP_EXP void
NewKeyer(double freq, BOOLEAN niambic, double gain, double ramp, double wpm, double SampleRate) {

  void *usemem;

 //------------------------------------------------------------

  gen = newCWToneGen(gain, freq, ramp, ramp, TONE_SIZE, SampleRate);

  //------------------------------------------------------------

  kl = newKeyerLogic();
  ks = newKeyerState();
  ks->flag.iambic = niambic;
  ks->flag.revpdl = TRUE; // depends on port wiring
  ks->flag.autospace.khar = ks->flag.autospace.word = FALSE;
  ks->debounce = 1; // could be more if sampled faster
  ks->mode = MODE_B;
  ks->weight = 50;
  ks->wpm = wpm;
  iambic = niambic;
  size = 2048;
  usemem = safealloc(1,4096*sizeof(float)+sizeof(ringb_t),"Keyer RB Left");
  lring = ringb_create(usemem, 4096*sizeof(float));
  usemem = safealloc(1,4096*sizeof(float)+sizeof(ringb_t),"Keyer RB Right");
  rring = ringb_create(usemem,4096*sizeof(float));
  ringb_clear(lring, size * sizeof(float));
  ringb_clear(rring, size * sizeof(float));
  sem_init(&clock_fired, 0, 0);
  sem_init(&poll_fired , 0, 0);
  sem_init(&keyer_started,0,0);
}

DttSP_EXP void
delKeyer() {
  sem_destroy(&clock_fired);
  sem_destroy(&poll_fired);
  sem_destroy(&keyer_started);
  delCWToneGen(gen);
  delKeyerState(ks);
  delKeyerLogic(kl);
  safefree((char *)lring);
  safefree((char *)rring);
}
DttSP_EXP void
KeyerClockFireWait()
{
	sem_wait(&clock_fired);
}
DttSP_EXP void
KeyerClockFireRelease()
{
	sem_post(&clock_fired);
}
DttSP_EXP void
KeyerStartedWait()
{
	sem_wait(&keyer_started);
}
DttSP_EXP void
KeyerStartedRelease()
{
	sem_post(&keyer_started);
}
DttSP_EXP void
PollTimerWait()
{
	sem_wait(&poll_fired);
}
DttSP_EXP void
PollTimerRelease()
{
	sem_post(&poll_fired);
}

//------------------------------------------------------------------------
