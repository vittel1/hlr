all: pointer.x map.x

%.x: %.o
	gcc -o $@ $<


%.o: %.c
	gcc -c $<


clean:
	rm -f *~
	rm -f map.x pointer.x
