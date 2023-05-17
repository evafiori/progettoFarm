CC			=  gcc

CFLAGS	    += -lpthread -std=c99 -Wall -pedantic -g
INCLUDEDIR	= -I ./headers

TARGETS		= farm generafile clean 

.PHONY: all clean test reset
.SUFFIXES: .c .h

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDEDIR) -c -o $@ $<

all: $(TARGETS)

reset: 		cleanExe clean

test: 		generafile farm
	./test.sh

generafile: ./source/generafile.c
	gcc -std=c99 -o $@ $^


farm: ./source/main.o ./source/myLibrary.o ./source/master.o  ./source/threadpool.o ./source/collector.o ./source/list.o ./source/tree.o
	$(CC) $(CFLAGS) $(INCLUDEDIR) -o $@ $^

./source/main.o:	./source/main.c

./source/myLibrary.o: 	./source/myLibrary.c 

./source/master.o: 	./source/master.c

./source/threadpool.o: 	./source/threadpool.c

./source/collector.o:	./source/collector.c

./source/list.o:	./source/list.c

./source/tree.o:	./source/tree.c

clean:
	\rm -f *.dat
	\rm -f *.txt
	\rm -r testdir
	\rm -f generafile
	\rm -f farm
	\rm -f ./src/*.o
	\rm -f ./farm.sck
	
	
