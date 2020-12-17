/* chan.h

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

#ifndef _chan_h
#define _chan_h

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <values.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <libgen.h>
#include <datatypes.h>
#include <banal.h>
#include <bufvec.h>
#include <ringb.h>

typedef
struct _chan {
  struct {
    size_t buf, rib, tot, tru;
  } size;
  struct {
    char *path;
    int desc;
  } file;
  struct {
    char *base;
  } map;
  ringb_t *rb;
} ChanDesc, *Chan;

extern size_t putChan(Chan c, char *data, size_t size);
extern size_t getChan(Chan c, char *data, size_t size);
extern BOOLEAN putChan_nowait(Chan c, char *data, size_t size);
extern size_t putChan_force(Chan c, char *data, size_t size);
extern BOOLEAN getChan_nowait(Chan c, char *data, size_t size);
extern void resetChan(Chan c);
// NB want will be rounded up to a power of 2
extern Chan openChan(char *path, size_t want);
extern void closeChan(Chan c);

#endif
