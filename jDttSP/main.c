/* main.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2004-5 by Frank Brickle, AB2KT and Bob McGwier, N4HY

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
// elementary defaults

struct _loc loc;  

/////////////////////////////////////////////////////////////////////////
// most of what little we know here about the inner loop,
// functionally speaking

extern void reset_meters(void);
extern void reset_spectrum(void);
extern void reset_counters(void);
extern void process_samples(float *, float *, float *, float *, int);
extern void setup_workspace(void);
extern void destroy_workspace(void);
extern void clear_jack_ringbuffer(jack_ringbuffer_t *rb, int nbytes);

//========================================================================

PRIVATE void
spectrum_thread(void) {

  while (top.running) {
    sem_wait(&top.sync.pws.sem);

    compute_spectrum(&uni.spec);

    if (fwrite((char *) &uni.spec.label, sizeof(int), 1, top.meas.spec.fp)
	!= 1) {
      fprintf(stderr, "error writing spectrum label\n");
      exit(1);
    }

    if (fwrite((char *) uni.spec.output, sizeof(float), uni.spec.size, top.meas.spec.fp)
	!= uni.spec.size) {
      fprintf(stderr, "error writing spectrum\n");
      exit(1);
    }

    if (fwrite((char *) uni.spec.oscope, sizeof(float), uni.spec.size, top.meas.spec.fp)
	!= uni.spec.size) {
      fprintf(stderr, "error writing oscope\n");
      exit(1);
    }

    fflush(top.meas.spec.fp);
  }

  pthread_exit(0);
}

PRIVATE void
meter_thread(void) {

  while (top.running) {
    sem_wait(&top.sync.mtr.sem);

    if (fwrite((char *) &uni.meter.label, sizeof(int), 1, top.meas.mtr.fp)
	!= 1) {
      fprintf(stderr, "error writing meter label\n");
      exit(1);
    }

    if (fwrite((char *) uni.meter.snap.rx, sizeof(REAL), MAXRX * RXMETERPTS, top.meas.mtr.fp)
	!= MAXRX * RXMETERPTS) {
      fprintf(stderr, "error writing rx meter\n");
      exit(1);
    }

    if (fwrite((char *) uni.meter.snap.tx, sizeof(REAL), TXMETERPTS, top.meas.mtr.fp)
	!= TXMETERPTS) {
      fprintf(stderr, "error writing tx meter\n");
      exit(1);
    }

    fflush(top.meas.mtr.fp);
  }

  pthread_exit(0);
}

//========================================================================

PRIVATE void
monitor_thread(void) {
  while (top.running) {
    sem_wait(&top.sync.mon.sem);
    fprintf(stderr,
	    "@@@ mon [%d]: cb = %d rbi = %d rbo = %d xr = %d\n",
	    uni.tick,
	    top.jack.blow.cb,
	    top.jack.blow.rb.i,
	    top.jack.blow.rb.o,
	    top.jack.blow.xr);
    memset((char *) &top.jack.blow, 0, sizeof(top.jack.blow));
  }
  pthread_exit(0);
}

//========================================================================

PRIVATE void 
process_updates_thread(void) {
  while (top.running) {
    pthread_testcancel();
    while (fgets(top.parm.buff, sizeof(top.parm.buff), top.parm.fp))
      do_update(top.parm.buff, top.verbose ? stderr : 0);
  }
  pthread_exit(0);
}

//========================================================================

PRIVATE void 
gethold(void) {
  if (jack_ringbuffer_write_space(top.jack.ring.o.l)
      < top.hold.size.bytes) {
    // pathology
    jack_ringbuffer_reset(top.jack.ring.o.l);
    jack_ringbuffer_reset(top.jack.ring.o.r);
    top.jack.blow.rb.o++;
  }
  jack_ringbuffer_write(top.jack.ring.o.l,
			(char *) top.hold.buf.l,
			top.hold.size.bytes);
  jack_ringbuffer_write(top.jack.ring.o.r,
			(char *) top.hold.buf.r,
			top.hold.size.bytes);
  if (jack_ringbuffer_read_space(top.jack.ring.i.l)
      < top.hold.size.bytes) {
    // pathology
    jack_ringbuffer_reset(top.jack.ring.i.l);
    jack_ringbuffer_reset(top.jack.ring.i.r);
    memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
    memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
    jack_ringbuffer_reset(top.jack.auxr.i.l);
    jack_ringbuffer_reset(top.jack.auxr.i.r);
    memset((char *) top.hold.aux.l, 0, top.hold.size.bytes);
    memset((char *) top.hold.aux.r, 0, top.hold.size.bytes);
    top.jack.blow.rb.i++;
  } else {
    jack_ringbuffer_read(top.jack.ring.i.l,
			 (char *) top.hold.buf.l,
			 top.hold.size.bytes);
    jack_ringbuffer_read(top.jack.ring.i.r,
			 (char *) top.hold.buf.r,
			 top.hold.size.bytes);
    jack_ringbuffer_read(top.jack.auxr.i.l,
			 (char *) top.hold.aux.l,
			 top.hold.size.bytes);
    jack_ringbuffer_read(top.jack.auxr.i.r,
			 (char *) top.hold.aux.r,
			 top.hold.size.bytes);
  }
}

PRIVATE BOOLEAN
canhold(void) {
  return
    jack_ringbuffer_read_space(top.jack.ring.i.l)
    >= top.hold.size.bytes;
}

//------------------------------------------------------------------------

PRIVATE void 
run_mute(void) {
  memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
  memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
  memset((char *) top.hold.aux.l, 0, top.hold.size.bytes);
  memset((char *) top.hold.aux.r, 0, top.hold.size.bytes);
  uni.tick++;
}

PRIVATE void 
run_pass(void) { uni.tick++; }

PRIVATE void 
run_play(void) {
  process_samples(top.hold.buf.l, top.hold.buf.r,
		  top.hold.aux.l, top.hold.aux.r,
		  top.hold.size.frames);
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
    switch (uni.mode.trx) {
      int i;
    case TX:
      tx.agc.gen->over = tx.tick + 3;
      break;
    case RX:
      for(i = 0; i < uni.multirx.nrx; i++)
	rx[i].agc.gen->over = rx[i].tick + 3;
      break;
    }

    top.state = top.swch.run.last;
    top.swch.bfct.want = top.swch.bfct.have = 0;

    jack_ringbuffer_reset(top.jack.ring.o.l);
    jack_ringbuffer_reset(top.jack.ring.o.r);
    clear_jack_ringbuffer(top.jack.ring.o.l, top.hold.size.bytes);
    clear_jack_ringbuffer(top.jack.ring.o.r, top.hold.size.bytes);

    reset_meters();
    reset_spectrum();
    reset_counters();
  }

  process_samples(top.hold.buf.l, top.hold.buf.r,
		  top.hold.aux.l, top.hold.aux.r,
		  top.hold.size.frames);
} 

//========================================================================

void
clear_jack_ringbuffer(jack_ringbuffer_t *rb, int nbytes) {
  int i;
  char zero = 0;
  for (i = 0; i < nbytes; i++)
    jack_ringbuffer_write(rb, &zero, 1);
}

PRIVATE void 
audio_callback(jack_nframes_t nframes, void *arg) {
  float *lp, *rp;
  int nbytes = nframes * sizeof(float);

  if (nframes == top.jack.size) {

    // output: copy from ring to port
    lp = (float *) jack_port_get_buffer(top.jack.port.o.l, nframes);
    rp = (float *) jack_port_get_buffer(top.jack.port.o.r, nframes);

    if (jack_ringbuffer_read_space(top.jack.ring.o.l) >= nbytes) {
      jack_ringbuffer_read(top.jack.ring.o.l, (char *) lp, nbytes);
      jack_ringbuffer_read(top.jack.ring.o.r, (char *) rp, nbytes);
    } else { // rb pathology
      memset((char *) lp, 0, nbytes);
      memset((char *) rp, 0, nbytes);
      jack_ringbuffer_reset(top.jack.ring.o.l);
      jack_ringbuffer_reset(top.jack.ring.o.r);
      clear_jack_ringbuffer(top.jack.ring.o.l, nbytes);
      clear_jack_ringbuffer(top.jack.ring.o.r, nbytes);
      top.jack.blow.rb.o++;
    }
    
    // input: copy from port to ring
    if (jack_ringbuffer_write_space(top.jack.ring.i.l) >= nbytes) {
      lp = (float *) jack_port_get_buffer(top.jack.port.i.l, nframes);
      rp = (float *) jack_port_get_buffer(top.jack.port.i.r, nframes);
      jack_ringbuffer_write(top.jack.ring.i.l, (char *) lp, nbytes);
      jack_ringbuffer_write(top.jack.ring.i.r, (char *) rp, nbytes);
      lp = (float *) jack_port_get_buffer(top.jack.auxp.i.l, nframes);
      rp = (float *) jack_port_get_buffer(top.jack.auxp.i.r, nframes);
      jack_ringbuffer_write(top.jack.auxr.i.l, (char *) lp, nbytes);
      jack_ringbuffer_write(top.jack.auxr.i.r, (char *) rp, nbytes);
    } else { // rb pathology
      jack_ringbuffer_reset(top.jack.ring.i.l);
      jack_ringbuffer_reset(top.jack.ring.i.r);
      clear_jack_ringbuffer(top.jack.ring.i.l, nbytes);
      clear_jack_ringbuffer(top.jack.ring.i.r, nbytes);
      jack_ringbuffer_reset(top.jack.auxr.i.l);
      jack_ringbuffer_reset(top.jack.auxr.i.r);
      clear_jack_ringbuffer(top.jack.auxr.i.l, nbytes);
      clear_jack_ringbuffer(top.jack.auxr.i.r, nbytes);
      top.jack.blow.rb.i++;
    }

  } else { // callback pathology
    jack_ringbuffer_reset(top.jack.ring.i.l);
    jack_ringbuffer_reset(top.jack.ring.i.r);
    jack_ringbuffer_reset(top.jack.ring.o.l);
    jack_ringbuffer_reset(top.jack.ring.o.r);
    clear_jack_ringbuffer(top.jack.ring.o.l, top.hold.size.bytes);
    clear_jack_ringbuffer(top.jack.ring.o.r, top.hold.size.bytes);
    top.jack.blow.cb++;
  }

  // if enough accumulated in ring, fire dsp
  if (jack_ringbuffer_read_space(top.jack.ring.i.l) >= top.hold.size.bytes)
    sem_post(&top.sync.buf.sem);

  // check for blowups
  if ((top.jack.blow.cb > 0) ||
      (top.jack.blow.rb.i > 0) ||
      (top.jack.blow.rb.o > 0))
    sem_post(&top.sync.mon.sem);
}

//========================================================================

PRIVATE void 
process_samples_thread(void) {
  while (top.running) {
    sem_wait(&top.sync.buf.sem);
    do {
      gethold();
      sem_wait(&top.sync.upd.sem);
      switch (top.state) {
      case RUN_MUTE: run_mute(); break;
      case RUN_PASS: run_pass(); break;
      case RUN_PLAY: run_play(); break;
      case RUN_SWCH: run_swch(); break;
      }
      sem_post(&top.sync.upd.sem);
    } while (canhold());
  }
  pthread_exit(0);
}

//========================================================================

PRIVATE void 
execute(void) {
  // let updates run
  sem_post(&top.sync.upd.sem);
  
  // rcfile
  {
    FILE *frc = find_rcfile(loc.path.rcfile);
    if (frc) {
      while (fgets(top.parm.buff, sizeof(top.parm.buff), frc))
	do_update(top.parm.buff, top.verbose ? stderr : 0);
      fclose(frc);
    }
  }

  // start audio processing
  if (jack_activate(top.jack.client))
    perror("cannot activate jack client"), exit(1);
  
  // wait for threads to terminate
  pthread_join(top.thrd.trx.id, 0);
  pthread_join(top.thrd.upd.id, 0);
  pthread_join(top.thrd.mon.id, 0);
  if (uni.meter.flag)
    pthread_join(top.thrd.mtr.id, 0);
  if (uni.spec.flag)
    pthread_join(top.thrd.pws.id, 0);
  
  // stop audio processing
  jack_client_close(top.jack.client);
}

PRIVATE void 
closeup(void) {
  jack_ringbuffer_free(top.jack.auxr.i.r);
  jack_ringbuffer_free(top.jack.auxr.i.l);
  jack_ringbuffer_free(top.jack.ring.o.r);
  jack_ringbuffer_free(top.jack.ring.o.l);
  jack_ringbuffer_free(top.jack.ring.i.r);
  jack_ringbuffer_free(top.jack.ring.i.l);

  safefree((char *) top.hold.buf.r);
  safefree((char *) top.hold.buf.l);
  safefree((char *) top.hold.aux.r);
  safefree((char *) top.hold.aux.l);

  fclose(top.parm.fp);

  if (uni.meter.flag)
    fclose(top.meas.mtr.fp);
  if (uni.spec.flag)
    fclose(top.meas.spec.fp);

  destroy_workspace();

  exit(0);
}

PRIVATE void 
usage(void) {
  fprintf(stderr, "usage:\n");
  fprintf(stderr, "jsdr [-flag [arg]] [file]\n");
  fprintf(stderr, "flags:\n");
  fprintf(stderr, "	-v		verbose commentary\n");
  fprintf(stderr, "	-m		do metering\n");
  fprintf(stderr, "	-s		do spectrum\n");
  fprintf(stderr, "	-l file		execute update commands in file at startup\n");
  fprintf(stderr, "'file' arg unused, but available\n");
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

//........................................................................

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
  top.hold.aux.l = (float *) safealloc(top.hold.size.frames, sizeof(float),
				       "aux hold buffer left");
  top.hold.aux.r = (float *) safealloc(top.hold.size.frames, sizeof(float),
				       "aux hold buffer right");
} 

PRIVATE void 
setup_updates(void) {
  top.parm.path = loc.path.parm;
  if ((top.parm.fd = open(top.parm.path, O_RDWR)) == -1)
    perror(top.parm.path), exit(1);
  if (!(top.parm.fp = fdopen(top.parm.fd, "r+"))) {
    fprintf(stderr, "can't fdopen parm pipe %s\n", loc.path.parm);
    exit(1);
  }

  // do this here 'cuz the update thread is controlling the action
  if (uni.meter.flag) {
    top.meas.mtr.path = loc.path.meter;
    top.meas.mtr.fp = efopen(top.meas.mtr.path, "r+");
  }
  if (uni.spec.flag) {
    top.meas.spec.path = loc.path.spec;
    top.meas.spec.fp = efopen(top.meas.spec.path, "r+");
  }
}

PRIVATE void
jack_xrun(void *arg) {
  top.jack.blow.xr++;
  sem_post(&top.sync.mon.sem);
}

PRIVATE void
jack_shutdown(void *arg) {}

PRIVATE void 
setup_system_audio(void) {
  if (loc.name[0]) strcpy(top.jack.name, loc.name);
  else sprintf(top.jack.name, "sdr-%d", top.pid);
  if (!(top.jack.client = jack_client_new(top.jack.name)))
    perror("can't make client -- jack not running?"), exit(1);

  jack_set_process_callback(top.jack.client, (void *) audio_callback, 0);
  jack_on_shutdown(top.jack.client, (void *) jack_shutdown, 0);
  jack_set_xrun_callback(top.jack.client, (void *) jack_xrun, 0);
  top.jack.size = jack_get_buffer_size(top.jack.client);
  memset((char *) &top.jack.blow, 0, sizeof(top.jack.blow));

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
  top.jack.auxp.i.l = jack_port_register(top.jack.client,
					 "al",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput,
					 0);
  top.jack.auxp.i.r = jack_port_register(top.jack.client,
					 "ar",
					 JACK_DEFAULT_AUDIO_TYPE,
					 JackPortIsInput,
					 0);
  top.jack.ring.i.l = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  top.jack.ring.i.r = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  top.jack.ring.o.l = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  top.jack.ring.o.r = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  top.jack.auxr.i.l = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  top.jack.auxr.i.r = jack_ringbuffer_create(top.hold.size.bytes * loc.mult.ring);
  clear_jack_ringbuffer(top.jack.ring.o.l, top.jack.size * sizeof(float));
  clear_jack_ringbuffer(top.jack.ring.o.r, top.jack.size * sizeof(float));
}

PRIVATE void 
setup_threading(void) {
  sem_init(&top.sync.upd.sem, 0, 0);
  pthread_create(&top.thrd.upd.id, NULL, (void *) process_updates_thread, NULL);
  sem_init(&top.sync.buf.sem, 0, 0);
  pthread_create(&top.thrd.trx.id, NULL, (void *) process_samples_thread, NULL);
  sem_init(&top.sync.mon.sem, 0, 0);
  pthread_create(&top.thrd.mon.id, NULL, (void *) monitor_thread, NULL);
  if (uni.meter.flag) {
    sem_init(&top.sync.mtr.sem, 0, 0);
    pthread_create(&top.thrd.mtr.id, NULL, (void *) meter_thread, NULL);
  }
  if (uni.spec.flag) {
    sem_init(&top.sync.pws.sem, 0, 0);
    pthread_create(&top.thrd.pws.id, NULL, (void *) spectrum_thread, NULL);
  }
} 

//========================================================================
// hard defaults, then environment

PRIVATE void
setup_defaults(void) {
  loc.name[0] = 0; // no default name for jack client
  strcpy(loc.path.rcfile, RCBASE);
  strcpy(loc.path.parm, PARMPATH);
  strcpy(loc.path.meter, METERPATH);
  strcpy(loc.path.spec, SPECPATH);
  strcpy(loc.path.wisdom, WISDOMPATH);
  loc.def.rate = DEFRATE;
  loc.def.size = DEFSIZE;
  loc.def.mode = DEFMODE;
  loc.def.spec = DEFSPEC;
  loc.def.comp = DEFCOMP;
  loc.def.nrx = MAXRX;
  loc.mult.ring = RINGMULT;

  {
    char *ep;
    if ((ep = getenv("SDR_NAME"))) strcpy(loc.name, ep);
    if ((ep = getenv("SDR_RCBASE"))) strcpy(loc.path.rcfile, ep);
    if ((ep = getenv("SDR_PARMPATH"))) strcpy(loc.path.parm, ep);
    if ((ep = getenv("SDR_METERPATH"))) strcpy(loc.path.meter, ep);
    if ((ep = getenv("SDR_SPECPATH"))) strcpy(loc.path.spec, ep);
    if ((ep = getenv("SDR_WISDOMPATH"))) strcpy(loc.path.wisdom, ep);
    if ((ep = getenv("SDR_RINGMULT"))) loc.mult.ring = atoi(ep);
    if ((ep = getenv("SDR_DEFRATE"))) loc.def.rate = atof(ep);
    if ((ep = getenv("SDR_DEFSIZE"))) loc.def.size = atoi(ep);
    if ((ep = getenv("SDR_DEFMODE"))) loc.def.mode = atoi(ep);
  }
}

//========================================================================
PRIVATE void 
setup(int argc, char **argv) {
  int i;

  top.uid = getuid();
  top.pid = getpid();
  top.start_tv = now_tv();
  top.running = TRUE;
  top.verbose = FALSE;
  top.state = RUN_PLAY;

  setup_defaults();
  
  for (i = 1; i < argc; i++)
    if (argv[i][0] == '-')
      switch (argv[i][1]) {
      case 'v':
	top.verbose = TRUE;
	break;
      case 'm':
	uni.meter.flag = TRUE;
	break;
      case 's':
	uni.spec.flag = TRUE;
	break;
      case 'l':
	strcpy(loc.path.rcfile, argv[++i]);
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
  setup_updates();

  setup_local_audio();
  setup_system_audio();

  setup_threading();
  setup_switching();
}

//========================================================================

int 
main(int argc, char **argv) { setup(argc, argv), execute(), closeup(); } 
