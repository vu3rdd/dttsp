CFLAGS = -g -O3 -I. -I/usr/local/include
#CFLAGS = -g -I. -I/usr/local/include
LIBS = -L/usr/local/lib -ljack -lpthread -lgsl -lgslcblas -lfftw -lm
#LIBS = -lefence -L/usr/local/lib -ljack -lpthread -lgsl -lgslcblas -lfftw -lm

staticlibname=libDttSP.a

OBJ =	am_demod.o\
	banal.o\
	bufvec.o\
	chan.o\
	chap.o\
	correctIQ.o\
	crc16.o\
	cxops.o\
	dcblock.o\
	digitalagc.o\
	fastrig.o\
	filter.o\
	fm_demod.o\
	lmadf.o\
	meter.o\
	noiseblanker.o\
	oscillator.o\
	ovsv.o\
	ringb.o\
	sdr.o\
	sdrexport.o\
	spectrum.o\
	speechproc.o\
	splitfields.o\
	spottone.o\
	thunk.o\
	window.o\
	wscompand.o\
	update.o

KOBJ = oscillator.o cwtones.o chan.o ringb.o banal.o bufvec.o splitfields.o cxops.o

jsdr:	main.o $(OBJ)
	$(CC) -o jsdr main.o $(OBJ) $(LIBS)

all:	jsdr ipc metermon specmon keyd keyb

keyd:	keyd.o keyer.o keyerio.o $(KOBJ)
	$(CC) -o keyd keyd.o keyer.o keyerio.o $(KOBJ) $(LIBS)

keyb:	keyb.o keyer.o keyerio.o $(KOBJ)
	$(CC) -o keyb keyb.o keyer.o $(KOBJ) $(LIBS)



$(OBJ): sdrexport.h

metermon:	metermon.o 
	$(CC) -o metermon metermon.o $(LIBS)

specmon:	specmon.o 
	$(CC) -o specmon specmon.o $(LIBS)

ipc:	
	./setup-ipc

obj:	$(OBJ)

clean:
	/bin/rm *.o jsdr keyb keyd metermon #$(staticlibname)
	#/bin/rm IPC/*

staticlib:	$(OBJ)
	ar rcs $(staticlibname) $(OBJ)
	ranlib $(staticlibname)

#mkchan:	mkchan.o bufvec.o banal.o cxops.o
#	$(CC) -o mkchan mkchan.o bufvec.o banal.o cxops.o $(LIBS)

#CFLAGS = -fPIC -g -O3 -I. -I/usr/local/include
# sharedlibname=libDttSP.so
# sharedlibvers=0.0.1
# sharedlib=$(sharedlibname).$(sharedlibvers)
# sharedlib:	$(OBJ)
# 	gcc -shared -Wl,-soname,$(sharedlib) -o $(sharedlib) $(OBJ) -lc

keybun:	keybun.o keyer.o keyerio.o $(KOBJ)
	$(CC) -o keybun keybun.o keyer.o $(KOBJ) $(LIBS)

