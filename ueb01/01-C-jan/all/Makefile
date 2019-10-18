all: pointer
pointer: pointer.o
	gcc -o pointer pointer.o
	
pointer.o: pointer.c
	gcc -c pointer.c

all: map
map: map.o
	gcc -o map map.o
	
map.o: map.c
	gcc -c map.c

clean:
	rm -f map
	rm -f *.o
	rm -f *~
	rm -f pointer
	rm -f *.o
	rm -f *~
