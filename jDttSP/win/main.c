/* main.c

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
  
#include <common.h>
  
/////////////////////////////////////////////////////////////////////////
extern void process_samples(float *, float *, int);
extern void setup_workspace(void);

#ifdef _WINDOWS
  
#define PARMPATH NULL
#define RINGMULT (4)
#define METERPATH NULL
#define METERMULT (2)
#define PWSPATH NULL
#define PWSMULT (32768)
#define TEST

#else
  
#define PARMPATH "./IPC/SDR-1000-0-commands.fifo"
#define RINGMULT (4)
#define METERPATH "./IPC/SDR-1000-0-meter.chan"
#define METERMULT (2)
#define PWSPATH "./IPC/SDR-1000-0-pws.chan"
#define PWSMULT (32768)
  
#endif
//------------------------------------------------------------------------
  
#ifdef _WINDOWS
PRIVATE DWORD WINAPI
process_updates_thread(void) {
  size_t tmp;
  while (top.running) {
    while (newupdates()) {
      tmp = ringb_read_space(uni.update.chan.c->rb);
      getChan_nowait(uni.update.chan.c, top.parm.buff, tmp);
      do_update(top.parm.buff);
    }
  }
  ExitThread(0);
}

//========================================================================

PRIVATE void 
gethold(void) {
  ringb_write(top.jack.ring.o.l, (char *) top.hold.buf.l, top.hold.size.bytes);
  ringb_write(top.jack.ring.o.r, (char *) top.hold.buf.r, top.hold.size.bytes);
  ringb_read(top.jack.ring.i.l, (char *) top.hold.buf.l, top.hold.size.bytes);
  ringb_read(top.jack.ring.i.r, (char *) top.hold.buf.r, top.hold.size.bytes);
}

PRIVATE BOOLEAN canhold(void) {
  return ringb_read_space(top.jack.ring.i.l) >= top.hold.size.bytes;
}


#else

//========================================================================

PRIVATE void 
process_updates_thread(void) {
  while (top.running) {
    while (fgets(top.parm.buff, sizeof(top.parm.buff), top.parm.fp))
      do_update(top.parm.buff, top.verbose ? stderr : 0);
  }
  pthread_exit(0);
}

PRIVATE void 
gethold(void) {
  jack_ringbuffer_write(top.jack.ring.o.l, (char *) top.hold.buf.l, top.hold.size.bytes);
  jack_ringbuffer_write(top.jack.ring.o.r, (char *) top.hold.buf.r, top.hold.size.bytes);
  jack_ringbuffer_read(top.jack.ring.i.l, (char *) top.hold.buf.l, top.hold.size.bytes);
  jack_ringbuffer_read(top.jack.ring.i.r, (char *) top.hold.buf.r, top.hold.size.bytes);
}

PRIVATE BOOLEAN canhold(void) {
  return jack_ringbuffer_read_space(top.jack.ring.i.l) >= top.hold.size.bytes;
}

#endif

PRIVATE void 
run_mute(void) {
  memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
  memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
}

PRIVATE void 
run_pass(void) {}

PRIVATE void 
run_play(void) {
  process_samples(top.hold.buf.l, top.hold.buf.r, top.hold.size.frames);
} 

// NB do not set RUN_SWCH directly via setRunState;
// use setSWCH instead

PRIVATE void 
run_swch(void) {
  if (top.swch.bfct.have == 0) {
    // first time
    // apply ramp down
    int i, m = top.swch.fade, n = top.swch.tail;
    for (i = 0; i < m; i++) {
      float w = (float) 1.0 - (float) i / m;
      top.hold.buf.l[i] *= w, top.hold.buf.r[i] *= w;
    }
    memset((char *) (top.hold.buf.l + m), 0, n);
    memset((char *) (top.hold.buf.r + m), 0, n);
    top.swch.bfct.have++;
  } else if (top.swch.bfct.have < top.swch.bfct.want) {
    // in medias res
    memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
    memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
    top.swch.bfct.have++;
  } else {
    // last time
    // apply ramp up
    int i, m = top.swch.fade, n = top.swch.tail;
    for (i = 0; i < m; i++) {
      float w = (float) i / m;
      top.hold.buf.l[i] *= w, top.hold.buf.r[i] *= w;
    }
    uni.mode.trx = top.swch.trx.next;
    top.state = top.swch.run.last;
    top.swch.bfct.want = top.swch.bfct.have = 0;
  }

  process_samples(top.hold.buf.l, top.hold.buf.r, top.hold.size.frames);
} 

//========================================================================

#ifdef _WINDOWS

static int exchptr = 0;
DttSP_EXP void 
exchange_samples(float *input, float *output, int nframes) {
  int i, j;
  for (i = 0, j = 0; i < nframes; i++, j += 2) {
    ringb_read(top.jack.ring.o.l, (char *) &output[j], sizeof(float));
    ringb_read(top.jack.ring.o.r, (char *) &output[j + 1], sizeof(float));
    ringb_write(top.jack.ring.i.l, (char *) &input[j], sizeof(float));
    ringb_write(top.jack.ring.i.r, (char *) &input[j + 1], sizeof(float));
  }
  if (ringb_read_space(top.jack.ring.i.l) >= top.hold.size.bytes)
    ReleaseSemaphore(top.sync.buf.sem, 1L, NULL);
}

DttSP_EXP void
audioreset(void) { exchptr = 0; }

PRIVATE DWORD WINAPI
process_samples_thread(void) {
  while (top.running) {
    WaitForSingleObject(top.sync.buf.sem, INFINITE);
    WaitForSingleObject(top.sync.upd.sem, INFINITE);
    do {
      gethold();
      switch (top.state) {
      case RUN_MUTE:
	run_mute();
	break;
      case RUN_PASS:
	run_pass();
	break;
      case RUN_PLAY:
	run_play();
	break;
      case RUN_SWCH:
	run_swch();
	break;
      }
    } while (canhold());
    ReleaseSemaphore(top.sync.upd.sem, 1L, NULL);
  }
  ExitThread(0);
  return 1;
}
  
#else	/*  */

PRIVATE void 
audio_callback(jack_nframes_t nframes, void *arg) {
  float *lp, *rp;
  int nbytes = nframes * sizeof(float);
  
  // output: copy from ring to port
  lp = (float *) jack_port_get_buffer(top.jack.port.o.l, nframes);
  rp = (float *) jack_port_get_buffer(top.jack.port.o.r, nframes);
  jack_ringbuffer_read(top.jack.ring.o.l, (char *) lp, nbytes);
  jack_ringbuffer_read(top.jack.ring.o.r, (char *) rp, nbytes);
  
  // input: copy from port to ring
  lp = (float *) jack_port_get_buffer(top.jack.port.i.l, nframes);
  rp = (float *) jack_port_get_buffer(top.jack.port.i.r, nframes);
  jack_ringbuffer_write(top.jack.ring.i.l, (char *) lp, nbytes);
  jack_ringbuffer_write(top.jack.ring.i.r, (char *) rp, nbytes);
  
  // if enough accumulated in ring, fire dsp
  if (jack_ringbuffer_read_space(top.jack.ring.i.l) >= top.hold.size.bytes)
    sem_post(&top.sync.buf.sem);
}

PRIVATE void 
process_samples_thread(void) {
  while (top.running) {
    sem_wait(&top.sync.buf.sem);
    do {
      gethold();
      sem_wait(&top.sync.upd.sem);
      switch (top.state) {
      case RUN_MUTE:
	run_mute();
	break;
      case RUN_PASS:
	run_pass();
	break;
      case RUN_PLAY:
	run_play();
	break;
      case RUN_SWCH:
	run_swch();
	break;
      }
      sem_post(&top.sync.upd.sem);
    } while (canhold());
  }
  pthread_exit(0);
}

#endif

//========================================================================

#ifdef _WINDOWS
DttSP_EXP  void 
execute(void) {
  ResumeThread(top.thrd.trx.thrd);
}

DttSP_EXP void
closeup(void) {
  extern void destroy_workspace();
  TerminateThread(top.thrd.trx.thrd, 0L);
  CloseHandle(top.thrd.trx.thrd);
  CloseHandle(top.thrd.upd.thrd);
  CloseHandle(top.sync.buf.sem);
  CloseHandle(top.sync.upd.sem);
  destroy_workspace();
} 

#else

PRIVATE void 
execute(void) {
  sem_post(&top.sync.upd.sem);
  
  // start audio processing
  if (jack_activate(top.jack.client))
    perror("cannot activate jack client"), exit(1);
  
  // wait for threads to terminate
  pthread_join(top.thrd.trx.id, 0);
  pthread_join(top.thrd.upd.id, 0);
  
  // stop audio processing
  jack_client_close(top.jack.client);
}

PRIVATE void 
closeup(void) { exit(0); }

PRIVATE void 
usage(void) {
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "-m\n");
  fprintf(stderr, "-v\n");
  exit(1);
}

PRIVATE void 
nonblock(int fd) {
  long arg;
  arg = fcntl(fd, F_GETFL);
  arg |= O_NONBLOCK;
/*  if (fcntl(fd, F_GETFL, &arg) >= 0)
    fcntl(fd, F_SETFL, arg | O_NONBLOCK); */ 
    fcntl(fd, F_SETFL, arg);
} 

#endif

//........................................................................

PRIVATE void 
setup_meter(void) {
  uni.meter.flag = TRUE;
  uni.meter.chan.path = METERPATH;
  uni.meter.chan.size = METERMULT * sizeof(REAL);
  uni.meter.val = -200.0;
  uni.meter.chan.c = openChan(uni.meter.chan.path, uni.meter.chan.size);
}

PRIVATE void 
setup_powerspectrum(void) {
  uni.powsp.flag = TRUE;
  uni.powsp.chan.path = PWSPATH;
  uni.powsp.chan.size = 8192 * sizeof(COMPLEX);
  uni.powsp.buf = newCXB(4096, NULL, "PowerSpectrum Chan Buffer");
  uni.powsp.fftsize = 4096;
  uni.powsp.chan.c = openChan(uni.powsp.chan.path, uni.powsp.chan.size);
}

PRIVATE void 
setup_switching(void) {
  top.swch.fade = (int) (0.1 * uni.buflen + 0.5);
  top.swch.tail = (top.hold.size.frames - top.swch.fade) * sizeof(float);
}

PRIVATE void 
setup_local_audio(void) {
  top.hold.size.frames = uni.buflen;
  top.hold.size.bytes = top.hold.size.frames * sizeof(float);
  top.hold.buf.l = (float *) safealloc(top.hold.size.frames, sizeof(float),
				       "main hold buffer left");
  top.hold.buf.r = (float *) safealloc(top.hold.size.frames, sizeof(float),
				       "main hold buffer right");
} 

#ifdef _WINDOWS

PRIVATE void 
setup_updates(void) {
  uni.update.flag = TRUE;
  uni.update.chan.path = NULL;
  uni.update.chan.size = 4096;
  uni.update.chan.c = openChan(uni.update.chan.path, uni.update.chan.size);
} PRIVATE void 

setup_system_audio(void) {
  sprintf(top.jack.name, "sdr-%d", top.pid);
  top.jack.ring.i.l = ringb_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.i.r = ringb_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.o.l = ringb_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.o.r = ringb_create(top.hold.size.bytes * RINGMULT);
  {
    unsigned int i;
    char zero = 0;
    for (i = 0; i < top.hold.size.bytes; i++) {
      ringb_write(top.jack.ring.o.l, &zero, 1);
      ringb_write(top.jack.ring.o.r, &zero, 1);
    }
  }
}

DttSP_EXP void
release_update(void) {}

DttSP_EXP void
suspend_sdr(void) { SuspendThread(top.thrd.trx.thrd); }

PRIVATE void 
setup_threading(void) {
  top.sync.upd.sem = CreateSemaphore(NULL, 1L, 1L, "UpdateSemaphore");
  top.sync.buf.sem = CreateSemaphore(NULL, 0L, 1L, "Process Samples Semaphore");
  top.thrd.trx.thrd = CreateThread((LPSECURITY_ATTRIBUTES) NULL, 0L, process_samples_thread,
				   NULL, CREATE_SUSPENDED, &top.thrd.trx.id);
  SetThreadPriority(top.thrd.trx.thrd, THREAD_PRIORITY_HIGHEST);
}

DttSP_EXP void
setup_sdr(void) {
  top.pid = GetCurrentThreadId();
  top.uid = 0L;
  top.running = TRUE;
  top.state = RUN_PLAY;
  setup_meter();
  setup_powerspectrum();
  setup_updates();
  uni.meter.flag = TRUE;
  top.verbose = FALSE;
  setup_workspace();
  setup_local_audio();
  setup_system_audio();
  setup_threading();
  setup_switching();
}

#else

PRIVATE void 
setup_updates(void) {
  top.parm.path = PARMPATH;
  if ((top.parm.fd = open(top.parm.path, O_RDWR)) == -1)
    perror(top.parm.path), exit(1);
  if (!(top.parm.fp = fdopen(top.parm.fd, "r+"))) {
    fprintf(stderr, "can't fdopen parm pipe %s\n", PARMPATH);
    exit(1);
  }
}

PRIVATE void 
setup_system_audio(void) {
  sprintf(top.jack.name, "sdr-%d", top.pid);
  if (!(top.jack.client = jack_client_new(top.jack.name)))
    perror("can't make client -- jack not running?"), exit(1);
  jack_set_process_callback(top.jack.client, (void *) audio_callback, 0);
  top.jack.port.i.l = jack_port_register(top.jack.client,
					 "il",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput,
					 0);
  top.jack.port.i.r = jack_port_register(top.jack.client,
					 "ir",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput,
					 0);
  top.jack.port.o.l = jack_port_register(top.jack.client,
					 "ol",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsOutput,
					 0);
  top.jack.port.o.r = jack_port_register(top.jack.client,
					 "or",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsOutput,
					 0);
  top.jack.ring.i.l = jack_ringbuffer_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.i.r = jack_ringbuffer_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.o.l = jack_ringbuffer_create(top.hold.size.bytes * RINGMULT);
  top.jack.ring.o.r = jack_ringbuffer_create(top.hold.size.bytes * RINGMULT);
  {
    int i;
    char zero = 0;
    for (i = 0; i < top.hold.size.bytes * RINGMULT / 2; i++) {
      jack_ringbuffer_write(top.jack.ring.o.l, &zero, 1);
      jack_ringbuffer_write(top.jack.ring.o.r, &zero, 1);
    }
  }
}

PRIVATE void 
setup_threading(void) {
  sem_init(&top.sync.upd.sem, 0, 0);
  pthread_create(&top.thrd.upd.id, NULL, (void *) process_updates_thread, NULL);
  sem_init(&top.sync.buf.sem, 0, 0);
  pthread_create(&top.thrd.trx.id, NULL, (void *) process_samples_thread, NULL);
} 

//========================================================================

PRIVATE void 
setup(int argc, char **argv) {
  int i;
  top.pid = getpid();
  top.uid = getuid();
  top.start_tv = now_tv();
  top.running = TRUE;
  top.verbose = FALSE;
  top.state = RUN_PLAY;
  setup_meter();
  setup_updates();
  for (i = 1; i < argc; i++)
    if (argv[i][0] == '-')
      switch (argv[i][1]) {
      case 'm':
	uni.meter.flag = TRUE;
	break;
      case 'v':
	top.verbose = TRUE;
	break;
      default:
	usage();
      }
    else break;
  if (i < argc) {
    if (!freopen(argv[i], "r", stdin))
      perror(argv[i]), exit(1);
    i++;
  }
  setup_workspace();
  setup_local_audio();
  setup_system_audio();
  setup_threading();
  setup_switching();
}


//========================================================================

int 
main(int argc, char **argv) {
  setup(argc, argv), execute(), closeup();
} 

#endif  
