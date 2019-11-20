CC = mpicc

CFLAGS = -std=c11 -pedantic -Wall -Wextra

TGTS = timempi
OBJS = timempi.o

all: $(TGTS)

timempi: timempi.o Makefile
	$(CC) $(CFLAGS) -o $@ $<

timempi.o: timempi.c Makefile

%.o: %.c
	$(CC) -c $(CFLAGS) $*.c


clean:
	$(RM) $(TGTS)
	$(RM) $(OBJS)
