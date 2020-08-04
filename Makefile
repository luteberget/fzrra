

brass: brass.c
	gcc -o brass brass.c -lsoundio -lpulse -pthread -lm

fmbase: fmbase.c
	gcc -o fmbase fmbase.c -lsoundio -lpulse -pthread -lm

fm: fm.c
	gcc -o fm fm.c -lsoundio -lpulse -pthread -lm

glocken: glocken.c
	gcc -o glocken glocken.c -lsoundio -lpulse -pthread -lm

env: env.c
	gcc -o env env.c libsoundio.a -I. -lpulse -pthread -lm
