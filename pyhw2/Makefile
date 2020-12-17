CFLAGS = -g -I. -I/usr/local/include
LDFLAGS =
LIBS = -lm


python:
	swig -python hardware.i
	gcc -fPIC -I. -O3 -I/usr/include/python -c hardware_wrap.c
	gcc -fPIC -I. -O3 -c hardware.c
	ld -shared hardware.o hardware_wrap.o -o _sdr1khw.so

demo:	demo.o hardware.o
	$(CC) $(LDFLAGS) -o demo demo.o hardware.o $(LIBS)
