CC = mpicc

CFLAGS = -std=c11 -pedantic -Wall -Wextra

TGTS = timempi timempi2
OBJS = timempi.o timempi2.o

all: $(TGTS)

timempi: timempi.o Makefile
	$(CC) $(CFLAGS) -o $@ $<

timempi.o: timempi.c Makefile

timempi2: timempi2.o Makefile
	$(CC) $(CFLAGS) -o $@ $<

timempi2.o: timempi2.c Makefile

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c


clean:
	$(RM) $(TGTS)
	$(RM) $(OBJS)
