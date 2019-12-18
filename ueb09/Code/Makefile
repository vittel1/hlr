CC = mpicc

CFLAGS = -std=c11 -Wall -Wextra -ggdb
LIBS = -lm

TGTS = partdiff-par
OBJS = partdiff.o askparams.o

all: partdiff-par

partdiff-par: $(OBJS) Makefile
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

partdiff.o: partdiff.c Makefile

askparams.o: askparams.c Makefile


%.o: %.c
	$(CC) -c $(CFLAGS) $*.c


clean:
	$(RM) $(OBJS)
	$(RM) $(TGTS)
