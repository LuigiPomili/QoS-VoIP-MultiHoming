GCCFLAGS= -Wall -Wunused -pedantic -ggdb

all: mobileLB fixedLB

mobileLB: mobileLB.o our_util.o list_util.o
	gcc -o mobileLB ${GCCFLAGS} mobileLB.o our_util.o list_util.o

mobileLB.o: mobileLB.c our_util.h list_util.h
	gcc -c ${GCCFLAGS} mobileLB.c

fixedLB: fixedLB.o our_util.o list_util.o
	gcc -o fixedLB ${GCCFLAGS} fixedLB.o our_util.o list_util.o

fixedLB.o: fixedLB.c our_util.h list_util.h
	gcc -c ${GCCFLAGS} fixedLB.c

our_util.o: our_util.c 
	gcc -c ${GCCFLAGS} our_util.c

list_util.o: list_util.c 
	gcc -c ${GCCFLAGS} list_util.c

clean:	
	rm -f *.o *.*.swp
	rm -f mobileLB fixedLB
