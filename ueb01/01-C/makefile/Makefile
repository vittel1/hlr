all: map pointer

map: map.c
	gcc -Wall -g -o map map.c

pointer: pointer.o
	gcc -o pointer pointer.o

pointer.o: pointer.c
	gcc -c pointer.c

clean:
	rm -f map
	rm -f pointer
	rm -f *.o
	rm -f *~
