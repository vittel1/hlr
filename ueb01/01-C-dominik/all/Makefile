files = pointer map

all: $(files)

% : %.o
	gcc -o $* $*.o

%.o : %.c
	gcc -c $*.c

clean:
	rm -f $(files)
	rm -f *.o

zip:
	tar -czf ../ubueng1.tar.gz .
