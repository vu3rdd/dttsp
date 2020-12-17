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

#include <linux/rtc.h>
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
#include <keyer.h>

#define SAMP_RATE (48000)

// # times key is sampled per sec
#define RTC_RATE (64)

// # samples generated during 1 clock tick at RTC_RATE
#define TONE_SIZE (SAMP_RATE / RTC_RATE)

// ring buffer size; > 1 sec at this sr
#define RING_SIZE (01 << 020)

KeyerState ks;
KeyerLogic kl;

pthread_t play, key, update;
sem_t clock_fired, keyer_started, update_ok;

int fdser, fdrtc;

jack_client_t *client;
jack_port_t *lport, *rport;
jack_ringbuffer_t *lring, *rring;
jack_nframes_t size;

CWToneGen gen;
BOOLEAN playing = FALSE, iambic = FALSE;
double wpm = 18.0, freq = 750.0, ramp = 5.0, gain = -3.0;

//------------------------------------------------------------

void
jack_ringbuffer_clear(jack_ringbuffer_t *ring, int nbytes) {
  int i;
  char zero = 0;
  for (i = 0; i < nbytes; i++)
    jack_ringbuffer_write(ring, &zero, 1);
}

void
jack_ringbuffer_restart(jack_ringbuffer_t *ring, int nbytes) {
  jack_ringbuffer_reset(ring);
  jack_ringbuffer_clear(ring, nbytes);
}

//------------------------------------------------------------

// generated tone -> output ringbuffer
void
send_tone(void) {
  if (jack_ringbuffer_write_space(lring) < TONE_SIZE * sizeof(float)) {
    write(2, "overrun tone\n", 13);
    jack_ringbuffer_restart(lring, TONE_SIZE * sizeof(float));
    jack_ringbuffer_restart(rring, TONE_SIZE * sizeof(float));
  } else {
    int i;
    for (i = 0; i < gen->size; i++) {
      float l = CXBreal(gen->buf, i),
	    r = CXBimag(gen->buf, i);
      jack_ringbuffer_write(lring, (char *) &l, sizeof(float));
      jack_ringbuffer_write(rring, (char *) &r, sizeof(float));
    }
  }
}

// silence -> output ringbuffer
void
send_silence(void) {
  if (jack_ringbuffer_write_space(lring) < TONE_SIZE * sizeof(float)) {
    write(2, "overrun zero\n", 13);
    jack_ringbuffer_restart(lring, TONE_SIZE * sizeof(float));
    jack_ringbuffer_restart(rring, TONE_SIZE * sizeof(float));
  } else {
    int i;
    for (i = 0; i < gen->size; i++) {
      float zero = 0.0;
      jack_ringbuffer_write(lring, (char *) &zero, sizeof(float));
      jack_ringbuffer_write(rring, (char *) &zero, sizeof(float));
    }
  }
}

//------------------------------------------------------------------------

// sound/silence generation
// tone turned on/off asynchronously

void
sound_thread(void) {
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
      sem_post(&update_ok);
    }
  }

  pthread_exit(0);
}

//------------------------------------------------------------------------

// basic heartbeat
// returns actual dur in msec since last tick;
// uses Linux rtc interrupts.
// other strategies will work too, so long as they
// provide a measurable delay in msec.

double
timed_delay(void) {
  double del, std = 1000 / (double) RTC_RATE;
  static int cnt = 0;
  unsigned long data;
  
  if (read(fdrtc, &data, sizeof(unsigned long)) == -1) {
    perror("read");
    exit(1);
  }
  // indicate whether an interrupt was missed
  // not really important except for performance tweaks
  if ((del = (data >> 010) * 1000 / (double) RTC_RATE) != std)
    fprintf(stderr, "%d %g ms\n", ++cnt, del);
  return del;
}

// key down? (real or via keyer logic)

BOOLEAN
read_key(double del) {
  if (iambic)
    return read_iambic_key_serial(ks, fdser, kl, del);
  else
    return read_straight_key_serial(ks, fdser);
}

//------------------------------------------------------------------------

// main keyer loop

void
key_thread(void) {

  sem_wait(&keyer_started);

  for (;;) {
    // wait for next tick, get actual dur since last one
    double del = timed_delay();
    // read key; tell keyer elapsed time since last call
    BOOLEAN keydown = read_key(del);

    if (!playing && keydown)
      CWToneOn(gen), playing = TRUE;
    else if (playing && !keydown)
      CWToneOff(gen);

    sem_post(&clock_fired);
  }

  pthread_exit(0);
}

//------------------------------------------------------------------------

// update keyer parameters via text input from stdin
// <wpm xxx>  -> set keyer speed to xxx
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

//------------------------------------------------------------------------

PRIVATE void
jack_xrun(void *arg) {
  char *str = "xrun";
  write(2, str, strlen(str));
}

PRIVATE void
jack_shutdown(void *arg) {}

PRIVATE void
jack_callback(jack_nframes_t nframes, void *arg) {
  float *lp, *rp;
  int nbytes = nframes * sizeof(float);
  if (nframes == size) {
    // output: copy from ring to port
    lp = (float *) jack_port_get_buffer(lport, nframes);
    rp = (float *) jack_port_get_buffer(rport, nframes);
    if (jack_ringbuffer_read_space(lring) >= nbytes) {
      jack_ringbuffer_read(lring, (char *) lp, nbytes);
      jack_ringbuffer_read(rring, (char *) rp, nbytes);
    } else { // rb pathology
      memset((char *) lp, 0, nbytes);
      memset((char *) rp, 0, nbytes);
      jack_ringbuffer_reset(lring);
      jack_ringbuffer_reset(rring);
      jack_ringbuffer_clear(lring, nbytes);
      jack_ringbuffer_clear(rring, nbytes);
      //write(2, "underrun\n", 9); 
    }
  }
}

int
main(int argc, char **argv) {
  int i;
  char *serialdev = "/dev/ttyS0",
       *clockdev = "/dev/rtc";
  int serstatus;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '-')
      switch (argv[i][1]) {
      case 'f':
	freq = atof(argv[++i]);
	break;
      case 'i':
	iambic = TRUE;
	break;
      case 'g':
	gain = atof(argv[++i]);
	break;
      case 'r':
	ramp = atof(argv[++i]);
	break;
      case 'w':
	wpm = atof(argv[++i]);
	break;
      default:
	fprintf(stderr,
		"keyd [-i] [-w wpm] [-g gain_dB] [-r ramp_ms]\n");
	exit(1);
      }
    else break;

  if (i < argc) {
    if (!freopen(argv[i], "r", stdin))
      perror(argv[i]), exit(1);
    i++;
  }

  //------------------------------------------------------------

  gen = newCWToneGen(gain, freq, ramp, ramp, TONE_SIZE, 48000.0);

  //------------------------------------------------------------

  kl = newKeyerLogic();
  ks = newKeyerState();
  ks->flag.iambic = TRUE;
  ks->flag.revpdl = TRUE; // depends on port wiring
  ks->flag.autospace.khar = ks->flag.autospace.word = FALSE;
  ks->debounce = 1; // could be more if sampled faster
  ks->mode = MODE_B;
  ks->weight = 50;
  ks->wpm = wpm;

  //------------------------------------------------------------

  if (!(client = jack_client_new("keyd")))
    fprintf(stderr, "can't make client -- jack not running?\n"), exit(1);
  jack_set_process_callback(client, (void *) jack_callback, 0);
  jack_on_shutdown(client, (void *) jack_shutdown, 0);
  jack_set_xrun_callback(client, (void *) jack_xrun, 0);
  size = jack_get_buffer_size(client);
  lport = jack_port_register(client,
			     "ol",
			     JACK_DEFAULT_AUDIO_TYPE,
			     JackPortIsOutput,
			     0);
  rport = jack_port_register(client,
			     "or",
			     JACK_DEFAULT_AUDIO_TYPE,
			     JackPortIsOutput,
			     0);
  lring = jack_ringbuffer_create(RING_SIZE);
  rring = jack_ringbuffer_create(RING_SIZE);
  jack_ringbuffer_clear(lring, size * sizeof(float));
  jack_ringbuffer_clear(rring, size * sizeof(float));
  
  //------------------------------------------------------------

  // key
  if ((fdser = open(serialdev, O_WRONLY)) == -1) {
    fprintf(stderr, "cannot open serial device %s", serialdev);
    exit(1);
  }
  if (ioctl(fdser, TIOCMGET, &serstatus) == -1) {
    close(fdser);
    fprintf(stderr, "cannot get serial device status");
    exit(1);
  }
  serstatus |= TIOCM_DTR;
  if (ioctl(fdser, TIOCMSET, &serstatus) == -1) {
    close(fdser);
    fprintf(stderr, "cannot set serial device status");
    exit(1);
  }

  // rtc
  if ((fdrtc = open(clockdev, O_RDONLY)) == -1) {
    perror(clockdev);
    exit(1);
  }
  if (ioctl(fdrtc, RTC_IRQP_SET, RTC_RATE) == -1) {
    perror("ioctl irqp");
    exit(1);
  }
  if (ioctl(fdrtc, RTC_PIE_ON, 0) == -1) {
    perror("ioctl pie on");
    exit(1);
  }

  //------------------------------------------------------------

  sem_init(&clock_fired, 0, 0);
  sem_init(&keyer_started, 0, 0);
  sem_init(&update_ok, 0, 0);
  pthread_create(&play, 0, (void *) sound_thread, 0);
  pthread_create(&key, 0, (void *) key_thread, 0);
  pthread_create(&update, 0, (void *) updater, 0);

  //------------------------------------------------------------

  jack_activate(client);
  {
    const char **ports;
    if (!(ports = jack_get_ports(client, 0, 0, JackPortIsPhysical | JackPortIsInput))) {
      fprintf(stderr, "can't find any physical playback ports\n");
      exit(1);
    }
    if (jack_connect(client, jack_port_name(lport), ports[0])) {
      fprintf(stderr, "can't connect left output\n");
      exit(1);
    }
    if (jack_connect(client, jack_port_name(rport), ports[1])) {
      fprintf(stderr, "can't connect left output\n");
      exit(1);
    }
    free(ports);
  }
  sem_post(&keyer_started);

  pthread_join(play, 0);
  pthread_join(key, 0);
  pthread_join(update, 0);
  jack_client_close(client);

  //------------------------------------------------------------

  if (ioctl(fdrtc, RTC_PIE_OFF, 0) == -1) {
    perror("ioctl pie off");
    exit(1);
  }
  close(fdrtc);
  close(fdser);

  jack_ringbuffer_free(lring);
  jack_ringbuffer_free(rring);

  sem_destroy(&clock_fired);
  sem_destroy(&keyer_started);

  delCWToneGen(gen);
  delKeyerState(ks);
  delKeyerLogic(kl);

  //------------------------------------------------------------

  exit(0);
}
