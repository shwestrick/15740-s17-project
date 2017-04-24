CC = gcc
AS = as
CFLAGS = -O -std=gnu11 -Wall -Wextra -Wno-unused-parameter
LFLAGS = -lrt -lm -lpthread

primes.o: concurrency.h primes.c
	$(CC) $(CFLAGS) -o primes.o concurrency.h primes.c $(LFLAGS)

incr.o: concurrency.h incr.c
	$(CC) $(CFLAGS) -o incr.o concurrency.h incr.c $(LFLAGS)

mmt.s: mmt.c
	$(CC) $(CFLAGS) -S -o mmt.s mmt.c $(LFLAGS)

mmt.o: mmt.c
	$(CC) $(CFLAGS) -o mmt.o mmt.c $(LFLAGS)

.PHONY: clean
clean:
	rm -f *.o
