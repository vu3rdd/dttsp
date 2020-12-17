/* fromsys.h
   stuff we need to import everywhere 
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

#ifndef _fromsys_h
#define _fromsys_h
#ifndef _WINDOWS 

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
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <libgen.h>

#include <pthread.h>
#include <semaphore.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#define DttSP_EXP

#else

#include <sys/types.h>
//#include <sys/param.h> // WINBLOWS
#define MAXPATHLEN (260-1 /* NULL */)
#include <sys/stat.h>
#include <time.h>
//#include <unistd.h>
#include <fcntl.h>

#include <stdlib.h>
#include <windows.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358928
#endif
#include <assert.h>
#define DttSP_EXP __declspec(dllexport)
#define DttSP_IMP __declspec(dllimport)

#define sem_wait(p) WaitForSingleObject(p,INFINITE)
#define sem_post(p) ReleaseSemaphore(p,1,NULL);
#define pthread_exit(p) ExitThread((DWORD)p)

#endif  // _WINDOWS
#endif // _fromsys_h
