
fm-epiano: fm-epiano.c
	gcc -Wall -o fm-epiano fm-epiano.c libsoundio.a -I. -lpulse -pthread -lm

brass: brass.c
	gcc -o brass brass.c -I. libsoundio.a -lpulse -pthread -lm

fm-ex1: fm-ex1.c
	gcc -o fm-ex1 fm-ex1.c libsoundio.a -I. -lpulse -pthread -lm

fmbase: fmbase.c
	gcc -o fmbase fmbase.c -lsoundio -lpulse -pthread -lm

fm: fm.c
	gcc -O3 --std=gnu17 -o fm fm.c libsoundio.a librtmidi.a -I. -lpulse -pthread -lm -lstdc++ -lasound

glocken: glocken.c
	gcc -o glocken glocken.c -lsoundio -lpulse -pthread -lm

env: env.c
	gcc -o env env.c libsoundio.a -I. -lpulse -pthread -lm
