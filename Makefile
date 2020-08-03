
fmbase: fmbase.c
	gcc -o fmbase fmbase.c libsoundio.a -I. -lpulse -pthread -lm

fm: fm.c
	gcc -o fm fm.c libsoundio.a -I. -lpulse -pthread -lm

glocken: glocken.c
	gcc -o glocken glocken.c libsoundio.a -I. -lpulse -pthread -lm

env: env.c
	gcc -o env env.c libsoundio.a -I. -lpulse -pthread -lm
