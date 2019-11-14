# Common definitions
CC = gcc

# Compiler flags, paths and libraries
CFLAGS = -std=c11 -pedantic -Wall -Wextra -O3 -ggdb
LFLAGS = $(CFLAGS)
LIBS   = -lm -lpthread

TGTS = partdiffSeq partdiff-posix
OBJS = partdiff.o askparams.o

# Targets ...
all: partdiff-posix partdiffSeq

partdiff-posix: $(OBJS) Makefile
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)


partdiffSeq: partdiffSeq.o askparams.o
	$(CC) $(LFLAGS) -o $@ $^ $(LIBS)


partdiff.o: partdiff.c Makefile

partdiffSeq.o: partdiffSeq.c Makefile

askparams.o: askparams.c Makefile

# Rule to create *.o from *.c
%.o: %.c
	$(CC) -c $(CFLAGS) $*.c

clean:
	$(RM) $(OBJS) partdiffSeq.o
	$(RM) $(TGTS)
