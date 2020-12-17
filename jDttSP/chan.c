/* chan.c

fast inter-user-process single-reader/single-writer channels
using a modified version of jack ringbuffers and memory-mapped files.
   
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

#include <chan.h>

size_t
putChan(Chan c, char *data, size_t size) {
  return ringb_write(c->rb, data, size);
}

size_t
getChan(Chan c, char *data, size_t size) {
  return ringb_read(c->rb, data, size);
}

BOOLEAN
putChan_nowait(Chan c, char *data, size_t size) {
  if (ringb_write_space(c->rb) >= size) {
    ringb_write(c->rb, data, size);
    return TRUE;
  } else return FALSE;
}

size_t
putChan_force(Chan c, char *data, size_t size) {
  if (ringb_write_space(c->rb) >= size) {
    ringb_write(c->rb, data, size);
    return size;
  } else {
    ringb_reset(c->rb);
    return ringb_write(c->rb, data, size);
  }
}

BOOLEAN
getChan_nowait(Chan c, char *data, size_t size) {
  if (ringb_read_space(c->rb) >= size) {
    ringb_read(c->rb, data, size);
    return TRUE;
  } else return FALSE;
}

void
resetChan(Chan c) { ringb_reset(c->rb); }

Chan
openChan(char *path, size_t want) {
  Chan c = (Chan) safealloc(sizeof(ChanDesc), 1, "Chan header");
  c->size.rib = sizeof(ringb_t);
  c->size.buf = nblock2(want);
  c->file.path = path;
  if ((c->file.desc = open(c->file.path, O_RDWR)) == -1) {
    fprintf(stderr, "can't open Chan file %s\n", c->file.path);
    exit(1);
  }
  c->size.tot = c->size.rib + c->size.buf;
  if ((c->size.tru = fdsize(c->file.desc)) < c->size.tot) {
    fprintf(stderr,
	    "Chan file %s is too small (%d) for required size (%s)\n",
	    c->file.path, c->size.tru, c->size.tot);
    exit(1);
  }
  if (!(c->map.base = (char *) mmap(0,
				    c->size.tot,
				    PROT_READ | PROT_WRITE,
				    MAP_SHARED,
				    c->file.desc,
				    0))) {
    fprintf(stderr, "can't memory map %s for Chan\n",
	    c->file.path);
    exit(1);
  }
  if (!(c->rb = ringb_create(c->map.base, c->size.buf))) {
    fprintf(stderr, "can't map RB for Chan\n");
    exit(1);
  }
  return c;
}

void
closeChan(Chan c) {
  if (c) {
    close(c->file.desc);
    munmap(c->map.base, c->size.tot);
    safefree((char *) c);
  }
}
