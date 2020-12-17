python:
	swig -python hardware.i
	gcc -fPIC -I. -I/usr/include/python -c hardware_wrap.c
	gcc -fPIC -I. -c hardware.c
	ld -shared hardware.o hardware_wrap.o -o _sdr1khw.so
clean:
	/bin/rm -f hardware_wrap.c sdr1khw.py *.o *.so *.pyc

