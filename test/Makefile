CC = gcc
AS = as
CFLAGS = -O -std=gnu11 -Wall -Wextra -Wno-unused-parameter
LFLAGS = -lrt -lm -lpthread

primes.o: utils.h primes.c
	$(CC) $(CFLAGS) -o primes.o utils.h primes.c $(LFLAGS)

primes.pg: utils.h primes.c
	$(CC) $(CFLAGS) -o primes.pg utils.h primes.c -pg $(LFLAGS)

hashing.o: utils.h hashing.c
	$(CC) $(CFLAGS) -o hashing.o utils.h hashing.c $(LFLAGS)

incr.o: utils.h incr.c
	$(CC) $(CFLAGS) -o incr.o utils.h incr.c $(LFLAGS)

mmt.s: mmt.c
	$(CC) $(CFLAGS) -S -o mmt.s mmt.c $(LFLAGS)

mmt.o: mmt.c
	$(CC) $(CFLAGS) -o mmt.o mmt.c $(LFLAGS)

.PHONY: clean
clean:
	rm -f *.o
