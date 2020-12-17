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

// Windows SHTUFF

PRIVATE CRITICAL_SECTION csobj;
PRIVATE CRITICAL_SECTION cs_updobj;
PRIVATE LPCRITICAL_SECTION cs;
PRIVATE LPCRITICAL_SECTION cs_upd;
PRIVATE BOOLEAN IC = FALSE;

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

//========================================================================

PRIVATE void
spectrum_thread(void)
{
  DWORD NumBytesWritten;
  while (top.running) {
    sem_wait(&top.sync.pws.sem);
    compute_spectrum(&uni.spec);
    WriteFile(top.meas.spec.fd, (LPVOID) & uni.spec.label,
	      sizeof(int), &NumBytesWritten, NULL);
    WriteFile(top.meas.spec.fd, (LPVOID) uni.spec.output,
	      sizeof(float) * uni.spec.size, &NumBytesWritten, NULL);
  }
  pthread_exit(0);
}

/*PRIVATE void
scope_thread(void) {
  DWORD NumBytesWritten;
  while (top.running) {
    sem_wait(&top.sync.scope.sem);
    compute_spectrum(&uni.spec);
	WriteFile(top.meas.scope.fd,(LPVOID)&uni.spec.label,
		sizeof(int),&NumBytesWritten,NULL);
	WriteFile(top.meas.scope.fd,(LPVOID)uni.spec.accum,
		sizeof(float)*uni.spec.size,&NumBytesWritten,NULL);
  }
  pthread_exit(0);
} */

PRIVATE void
meter_thread(void)
{
  DWORD NumBytesWritten;
  while (top.running) {
    sem_wait(&top.sync.mtr.sem);
    WriteFile(top.meas.mtr.fd, (LPVOID) & uni.meter.label, sizeof(int),
	      &NumBytesWritten, NULL);
    WriteFile(top.meas.mtr.fd, (LPVOID) & uni.meter.snap.rx,
	      sizeof(REAL) * MAXRX * RXMETERPTS, &NumBytesWritten, NULL);
    WriteFile(top.meas.mtr.fd, (LPVOID) & uni.meter.snap.tx,
	      sizeof(REAL) * TXMETERPTS, &NumBytesWritten, NULL);
  }
  pthread_exit(0);
}

//========================================================================

PRIVATE void
monitor_thread(void)
{
  while (top.running) {
    sem_wait(&top.sync.mon.sem);
    /* If there is anything that needs monitoring, do it here */
    fprintf(stderr,
	    "@@@ mon [%d]: cb = %d rbi = %d rbo = %d xr = %d\n",
	    uni.tick,
	    top.jack.blow.cb,
	    top.jack.blow.rb.i, top.jack.blow.rb.o, top.jack.blow.xr);
    memset((char *) &top.jack.blow, 0, sizeof(top.jack.blow));
  }
  pthread_exit(0);

}

//========================================================================

PRIVATE void
process_updates_thread(void)
{

  while (top.running) {
    DWORD NumBytesRead;
    pthread_testcancel();
    while (ReadFile(top.parm.fd, top.parm.buff, 256, &NumBytesRead, NULL)) {
      fprintf(stderr, "Update Bytes:%lu Msg:%s\n", NumBytesRead,
	      top.parm.buff), fflush(stderr);
      if (NumBytesRead != 0)
	do_update(top.parm.buff, top.verbose ? stderr : 0);
    }
  }
  pthread_exit(0);
}

//========================================================================


PRIVATE void
gethold(void)
{
  EnterCriticalSection(cs);
  if (ringb_write_space(top.jack.ring.o.l)
      < top.hold.size.bytes) {
    // pathology
    ringb_reset(top.jack.ring.o.l);
    ringb_reset(top.jack.ring.o.r);
    top.jack.blow.rb.o++;
  }
  ringb_write(top.jack.ring.o.l,
	      (char *) top.hold.buf.l, top.hold.size.bytes);
  ringb_write(top.jack.ring.o.r,
	      (char *) top.hold.buf.r, top.hold.size.bytes);
  if (ringb_read_space(top.jack.ring.i.l)
      < top.hold.size.bytes) {
    // pathology
    ringb_reset(top.jack.ring.i.l);
    ringb_reset(top.jack.ring.i.r);
    memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
    memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
    ringb_reset(top.jack.auxr.i.l);
    ringb_reset(top.jack.auxr.i.r);
    memset((char *) top.hold.aux.l, 0, top.hold.size.bytes);
    memset((char *) top.hold.aux.r, 0, top.hold.size.bytes);
    top.jack.blow.rb.i++;
  } else {
    ringb_read(top.jack.ring.i.l,
	       (char *) top.hold.buf.l, top.hold.size.bytes);
    ringb_read(top.jack.ring.i.r,
	       (char *) top.hold.buf.r, top.hold.size.bytes);
    ringb_read(top.jack.auxr.i.l,
	       (char *) top.hold.aux.l, top.hold.size.bytes);
    ringb_read(top.jack.auxr.i.r,
	       (char *) top.hold.aux.r, top.hold.size.bytes);
  }
  LeaveCriticalSection(cs);
}

PRIVATE BOOLEAN
canhold(void)
{
  BOOLEAN answer;
  EnterCriticalSection(cs);
  answer = (ringb_read_space(top.jack.ring.i.l) >= top.hold.size.bytes);
  LeaveCriticalSection(cs);
  return answer;
}


//------------------------------------------------------------------------

PRIVATE void
run_mute(void)
{
  memset((char *) top.hold.buf.l, 0, top.hold.size.bytes);
  memset((char *) top.hold.buf.r, 0, top.hold.size.bytes);
  memset((char *) top.hold.aux.l, 0, top.hold.size.bytes);
  memset((char *) top.hold.aux.r, 0, top.hold.size.bytes);
  uni.tick++;
}

PRIVATE void
run_pass(void)
{
  uni.tick++;
}

PRIVATE void
run_play(void)
{
  process_samples(top.hold.buf.l, top.hold.buf.r,
		  top.hold.aux.l, top.hold.aux.r, top.hold.size.frames);
}

// NB do not set RUN_SWCH directly via setRunState;
// use setSWCH instead

PRIVATE void
run_swch(void)
{
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
      for (i = 0; i < uni.multirx.nrx; i++)
	rx[i].agc.gen->over = rx[i].tick + 3;
      break;
    }

    top.state = top.swch.run.last;
    top.swch.bfct.want = top.swch.bfct.have = 0;

    ringb_reset(top.jack.ring.o.l);
    ringb_reset(top.jack.ring.o.r);
    ringb_clear(top.jack.ring.o.l, top.hold.size.bytes);
    ringb_clear(top.jack.ring.o.r, top.hold.size.bytes);

    reset_meters();
    reset_spectrum();
    reset_counters();
  }

  process_samples(top.hold.buf.l, top.hold.buf.r,
		  top.hold.aux.l, top.hold.aux.r, top.hold.size.frames);
}

//========================================================================



DttSP_EXP void
audio_callback(float *input_l, float *input_r, float *output_l,
	       float *output_r, int nframes)
{
  size_t nbytes = sizeof(float) * nframes;


  EnterCriticalSection(cs);


  if (ringb_read_space(top.jack.ring.o.l) >= nbytes) {
    ringb_read(top.jack.ring.o.l, (char *) output_l, nbytes);
    ringb_read(top.jack.ring.o.r, (char *) output_r, nbytes);
  } else {			// rb pathology
    memset((char *) output_l, 0, nbytes);
    memset((char *) output_r, 0, nbytes);
    ringb_restart(top.jack.ring.o.l, nbytes);
    ringb_restart(top.jack.ring.o.r, nbytes);
    top.jack.blow.rb.o++;
  }

  // input: copy from port to ring
  if (ringb_write_space(top.jack.ring.i.l) >= nbytes) {
    ringb_write(top.jack.ring.i.l, (char *) input_l, nbytes);
    ringb_write(top.jack.ring.i.r, (char *) input_r, nbytes);
    ringb_write(top.jack.auxr.i.l, (char *) input_l, nbytes);
    ringb_write(top.jack.auxr.i.r, (char *) input_r, nbytes);
  } else {			// rb pathology
    ringb_restart(top.jack.ring.i.l, nbytes);
    ringb_restart(top.jack.ring.i.r, nbytes);
    ringb_restart(top.jack.auxr.i.l, nbytes);
    ringb_restart(top.jack.auxr.i.r, nbytes);
    top.jack.blow.rb.i++;
  }
  LeaveCriticalSection(cs);
  // if enough accumulated in ring, fire dsp
  if (ringb_read_space(top.jack.ring.i.l) >= top.hold.size.bytes)
    sem_post(&top.sync.buf.sem);

  // check for blowups
  if ((top.jack.blow.cb > 0) ||
      (top.jack.blow.rb.i > 0) || (top.jack.blow.rb.o > 0))
    sem_post(&top.sync.mon.sem);
}

//========================================================================

DttSP_EXP void
process_samples_thread(void)
{
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
}


void
closeup(void)
{
  top.running = FALSE;
  Sleep(50);
  safefree((char *) top.jack.ring.o.r);
  safefree((char *) top.jack.ring.o.l);
  safefree((char *) top.jack.ring.i.r);
  safefree((char *) top.jack.ring.i.l);
  safefree((char *) top.jack.auxr.i.l);
  safefree((char *) top.jack.auxr.i.r);
  safefree((char *) top.jack.auxr.o.l);
  safefree((char *) top.jack.auxr.o.r);

  CloseHandle(top.parm.fp);
  DisconnectNamedPipe(top.parm.fd);
  CloseHandle(top.parm.fd);


  if (uni.meter.flag) {
    CloseHandle(top.meas.mtr.fp);
    DisconnectNamedPipe(top.meas.mtr.fd);
    CloseHandle(top.meas.mtr.fd);
  };

  if (uni.spec.flag) {
    CloseHandle(top.meas.spec.fp);
    DisconnectNamedPipe(top.meas.spec.fd);
    CloseHandle(top.meas.spec.fd);
  };
  destroy_workspace();
}

//........................................................................

PRIVATE void
setup_switching(void)
{
  top.swch.fade = (int) (0.1 * uni.buflen + 0.5);
  top.swch.tail = (top.hold.size.frames - top.swch.fade) * sizeof(float);
}

PRIVATE void
setup_local_audio(void)
{
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

#include <lmerr.h>

PRIVATE void
DisplayErrorText(DWORD dwLastError)
{
  HMODULE hModule = NULL;	// default to system source
  LPSTR MessageBuffer;
  DWORD dwBufferLength;

  DWORD dwFormatFlags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;

  //
  // If dwLastError is in the network range, 
  //  load the message source.
  //

  if (dwLastError >= NERR_BASE && dwLastError <= MAX_NERR) {
    hModule = LoadLibraryEx(TEXT("netmsg.dll"),
			    NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (hModule != NULL)
      dwFormatFlags |= FORMAT_MESSAGE_FROM_HMODULE;
  }
  //
  // Call FormatMessage() to allow for message 
  //  text to be acquired from the system 
  //  or from the supplied module handle.
  //

  if (dwBufferLength = FormatMessageA(dwFormatFlags, hModule,	// module to get message from (NULL == system)
				      dwLastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),	// default language
				      (LPSTR) & MessageBuffer, 0, NULL)) {
    DWORD dwBytesWritten;

    //
    // Output message string on stderr.
    //
    WriteFile(GetStdHandle(STD_ERROR_HANDLE),
	      MessageBuffer, dwBufferLength, &dwBytesWritten, NULL);

    //
    // Free the buffer allocated by the system.
    //
    LocalFree(MessageBuffer);
  }
  //
  // If we loaded a message source, unload it.
  //
  if (hModule != NULL)
    FreeLibrary(hModule);
}



PRIVATE sem_t setup_update_sem;

PRIVATE void
setup_update_server()
{

  if (INVALID_HANDLE_VALUE == (top.parm.fd = CreateNamedPipe(top.parm.path,
							     PIPE_ACCESS_INBOUND,
							     PIPE_WAIT |
							     PIPE_TYPE_MESSAGE
							     |
							     PIPE_READMODE_MESSAGE,
							     PIPE_UNLIMITED_INSTANCES,
							     512, 512,
							     INFINITE,
							     NULL))) {
    fprintf(stderr, "Update server pipe setup failed:\n"), fflush(stderr);
    DisplayErrorText(GetLastError());
  }
//  fprintf(stderr,"Update NamedPipe made\n"),fflush(stderr);
  sem_post(&setup_update_sem);
  if (ConnectNamedPipe(top.parm.fd, NULL)) {
//      fprintf(stderr,"Connected the server to the Update pipe\n"),fflush(stderr);
  } else {
    fprintf(stderr, "Connected the server to the Update pipe failed\n"),
      fflush(stderr);
    DisplayErrorText(GetLastError());
  }
  pthread_exit(0);
}


PRIVATE void
setup_update_client()
{
//      fprintf(stderr,"Looking for the Update server\n"),fflush(stderr);
  WaitNamedPipe(top.parm.path, INFINITE);
//      fprintf(stderr,"Found the Update server\n"),fflush(stderr);
  if (INVALID_HANDLE_VALUE == (top.parm.fp = CreateFile(top.parm.path,
							GENERIC_WRITE, 0,
							NULL, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL))) {
    fprintf(stderr, "The Update Client Open Failed\n"), fflush(stderr);
    DisplayErrorText(GetLastError());
  }
  sem_post(&setup_update_sem);
/*	{
		DWORD numwritten;
		WriteFile(top.parm.fp,"test",5,&numwritten,NULL);
		fprintf(stderr,"Number written to server: %lu\n",numwritten),fflush(stderr);
	}*/
  pthread_exit(0);
}

PRIVATE void
setup_meter_server()
{
  top.meas.mtr.fd = CreateNamedPipe(top.meas.mtr.path,
				    PIPE_ACCESS_OUTBOUND,
				    PIPE_WAIT | PIPE_TYPE_MESSAGE |
				    PIPE_READMODE_MESSAGE,
				    PIPE_UNLIMITED_INSTANCES, 512, 512,
				    INFINITE, NULL);
//  fprintf(stderr,"meter handle = %08X\n",(DWORD)top.meas.mtr.fd),fflush(stderr);
  if (top.meas.mtr.fd == INVALID_HANDLE_VALUE) {
    fprintf(stderr, "Meter server pipe setup failed:\n"), fflush(stderr);
    DisplayErrorText(GetLastError());
  } else {
//        fprintf(stderr,"Meter Pipe Connect succeeded\n"),fflush(stderr);
    sem_post(&setup_update_sem);
    if (ConnectNamedPipe(top.meas.mtr.fd, NULL)) {
//          fprintf(stderr,"Connected the Meter Pooch\n"),fflush(stderr);
    } else {
      fprintf(stderr, "Meter Pipe Connect failed\n"), fflush(stderr);
      DisplayErrorText(GetLastError());
    }
  }
  pthread_exit(0);
}

PRIVATE void
setup_meter_client()
{
//      fprintf(stderr,"Looking for the meter server\n"),fflush(stderr);
  if (WaitNamedPipe(top.meas.mtr.path, INFINITE)) {
//        fprintf(stderr,"Found the Meter server\n"),fflush(stderr);
    if (INVALID_HANDLE_VALUE ==
	(top.meas.mtr.fp =
	 CreateFile(top.meas.mtr.path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL, NULL))) {
      fprintf(stderr, "The Meter Client Open Failed\n"), fflush(stderr);
      DisplayErrorText(GetLastError());
    } else {
//                      fprintf(stderr,"The Meter Client Open Succeeded\n"),fflush(stderr);
    }
  } else {
    fprintf(stderr, "Wait for meter pipe failed: Error message %d\n",
	    GetLastError()), fflush(stderr);
  }
  sem_post(&setup_update_sem);
  pthread_exit(0);
}

PRIVATE void
setup_spec_server()
{

  if (INVALID_HANDLE_VALUE ==
      (top.meas.spec.fd =
       CreateNamedPipe(top.meas.spec.path, PIPE_ACCESS_OUTBOUND,
		       PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
		       PIPE_UNLIMITED_INSTANCES, 32768, 32768, INFINITE,
		       NULL))) {
    fprintf(stderr, "Spectrum pipe create failed\n"), fflush(stderr);
    DisplayErrorText(GetLastError());
  } else {
//        fprintf(stderr,"Spectrum Pipe %s Create succeeded\n",top.meas.spec.path),fflush(stderr);
    sem_post(&setup_update_sem);
    if (ConnectNamedPipe(top.meas.spec.fd, NULL)) {
//          fprintf(stderr,"Connected to the Spectrum Pipe\n"),fflush(stderr);
    } else {
      fprintf(stderr, "Spectrum pipe connect failed\n"), fflush(stderr);
      DisplayErrorText(GetLastError());
    }
  }
  pthread_exit(0);
}

PRIVATE void
setup_spec_client()
{
//      fprintf(stderr,"Looking for the spectrum server\n"),fflush(stderr);
  if (WaitNamedPipe(top.meas.spec.path, INFINITE)) {
//              fprintf(stderr,"Found the server\n"),fflush(stderr);
    if (INVALID_HANDLE_VALUE ==
	(top.meas.spec.fp =
	 CreateFile(top.meas.spec.path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
		    FILE_ATTRIBUTE_NORMAL, NULL))) {
      fprintf(stderr, "The Spectrum Client Open Failed\n"), fflush(stderr);
      DisplayErrorText(GetLastError());
    } else {
//                      fprintf(stderr,"The Spectrum Client Open Succeeded\n");
//                      fprintf(stderr,"Spec Read handle = %08X\n",(DWORD)top.meas.spec.fp),fflush(stderr);
    }
  } else {
    fprintf(stderr, "Wait for spec pipe failed\n"), fflush(stderr);
    DisplayErrorText(GetLastError());
  }
  sem_post(&setup_update_sem);
  pthread_exit(0);
}
PRIVATE pthread_t id1, id2, id3, id4, id5, id6;
PRIVATE void
setup_updates(void)
{

  char mesg[16384] = "TEST TEST METER\n";
//  DWORD NumBytes;
  top.parm.path = loc.path.parm;
  sem_init(&setup_update_sem, 0, 0);


  if (uni.meter.flag) {
    top.meas.mtr.path = loc.path.meter;
  }
  if (uni.spec.flag) {
    top.meas.spec.path = loc.path.spec;
  }

  // Do this STUPID stuff to make use of the Named Pipe Mechanism in Windows
  // For the update server


  pthread_create(&id1, NULL, (void *) setup_update_server, NULL);
  sem_wait(&setup_update_sem);
  pthread_create(&id2, NULL, (void *) setup_update_client, NULL);
  sem_wait(&setup_update_sem);
  if (uni.meter.flag) {
    pthread_create(&id3, NULL, (void *) setup_meter_server, NULL);
    sem_wait(&setup_update_sem);
    pthread_create(&id4, NULL, (void *) setup_meter_client, NULL);
    sem_wait(&setup_update_sem);
/*	  if (WriteFile(top.meas.mtr.fd,mesg,strlen(mesg)+1,&NumBytes,NULL))
	  {
		  fprintf(stderr,"Meter Pipe write succeeded and wrote %lu bytes\n",NumBytes),fflush(stderr);
	  } else {
		  fprintf(stderr,"Meter Pipe write failed\n"),fflush(stderr);
		  DisplayErrorText(GetLastError());
	  }
	  if (ReadFile(top.meas.mtr.fp,mesg,256,&NumBytes,NULL))
	  {
		  fprintf(stderr,"Meter Pipe read succeeded and %lu bytes read\n",NumBytes),fflush(stderr);
		  fprintf(stderr,"Meter message %s",mesg),fflush(stderr);
	  } else {
		  fprintf(stderr,"Meter Pipe read failed\n"),fflush(stderr);
		  DisplayErrorText(GetLastError());
	  }*/

  }

  if (uni.spec.flag) {
    memset(mesg, 0, 16384);
    pthread_create(&id5, NULL, (void *) setup_spec_server, NULL);
    sem_wait(&setup_update_sem);
    pthread_create(&id6, NULL, (void *) setup_spec_client, NULL);
    sem_wait(&setup_update_sem);
    Sleep(0);
/*	  if (WriteFile(top.meas.spec.fd,mesg,16384,&NumBytes,NULL))
	  {
		  fprintf(stderr,"Spec Pipe write succeeded and wrote %lu bytes\n",NumBytes),fflush(stderr);
	  } else {
		  fprintf(stderr,"Spec Pipe write failed\n"),fflush(stderr);
		  DisplayErrorText(GetLastError());
	  }
 	  fprintf(stderr,"Spec Read handle(2) = %08X\n",(DWORD)top.meas.spec.fp),fflush(stderr);
	  if (ReadFile(top.meas.spec.fp,mesg,16384,&NumBytes,NULL))
	  {
		  fprintf(stderr,"Spec Pipe read succeeded and %lu bytes read\n",NumBytes),fflush(stderr);
	  } else {
		  fprintf(stderr,"Spec Pipe read failed\n"),fflush(stderr);
		  DisplayErrorText(GetLastError());
	  } */
  }
  sem_destroy(&setup_update_sem);
}
PRIVATE void
setup_system_audio(void)
{
  size_t ringsize;
  void *usemem;
  sprintf(top.jack.name, "sdr-%d", top.pid);
  top.jack.size = uni.buflen;
  ringsize = top.hold.size.bytes * loc.mult.ring + sizeof(ringb_t);
  usemem = safealloc(ringsize, 1, "Ring Input Left");
  top.jack.ring.i.l =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Input Right");
  top.jack.ring.i.r =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Output Left");
  top.jack.ring.o.l =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Output Right");
  top.jack.ring.o.r =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Input Left Auxiliary");
  top.jack.auxr.i.l =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Input Right Auxiliary");
  top.jack.auxr.i.r =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Output Left Auxiliary");
  top.jack.auxr.o.l =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  usemem = safealloc(ringsize, 1, "Ring Output Right Auxiliary");
  top.jack.auxr.o.r =
    ringb_create(usemem, top.hold.size.bytes * loc.mult.ring);

  ringb_clear(top.jack.ring.o.l, top.jack.size * sizeof(float));
  ringb_clear(top.jack.ring.o.r, top.jack.size * sizeof(float));
  ringb_clear(top.jack.auxr.o.l, top.jack.size * sizeof(float));
  ringb_clear(top.jack.auxr.o.r, top.jack.size * sizeof(float));
}

PRIVATE void
setup_threading(void)
{
  sem_init(&top.sync.upd.sem, 0, 0);
  pthread_create(&top.thrd.upd.id, NULL, (void *) process_updates_thread,
		 NULL);
  sem_init(&top.sync.buf.sem, 0, 0);
  pthread_create(&top.thrd.trx.id, NULL, (void *) process_samples_thread,
		 NULL);
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
  cs = &csobj;
  InitializeCriticalSection(cs);
}

//========================================================================
// hard defaults, then environment

PRIVATE void
setup_defaults(void)
{
  loc.name[0] = 0;		// no default name for jack client
  strcpy(loc.path.rcfile, RCBASE);
  strcpy(loc.path.parm, PARMPATH);
  strcpy(loc.path.meter, METERPATH);
  strcpy(loc.path.spec, SPECPATH);
  strcpy(loc.path.wisdom, WISDOMPATH);
  loc.def.rate = DEFRATE;
  loc.def.size = DEFSIZE;
  loc.def.nrx = MAXRX;
  loc.def.mode = DEFMODE;
  loc.def.spec = DEFSPEC;
  loc.mult.ring = RINGMULT;
}

//========================================================================
void
setup()
{


  top.pid = GetCurrentThreadId();
  top.uid = 0L;
  top.start_tv = now_tv();
  top.running = TRUE;
  top.verbose = FALSE;
  top.state = RUN_PLAY;

  setup_defaults();
  top.verbose = FALSE;
  uni.meter.flag = TRUE;
  uni.spec.flag = TRUE;

  setup_workspace();
  setup_updates();

  setup_local_audio();
  setup_system_audio();

  setup_threading();
  setup_switching();
  uni.spec.flag = TRUE;
  uni.spec.type = SPEC_POST_FILT;
  uni.spec.scale = SPEC_PWR;
  uni.spec.rxk = 0;

}
