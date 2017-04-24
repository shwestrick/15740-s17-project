CC = gcc
AS = as
CFLAGS = -O -std=gnu11 -Wall -Wextra -Wno-unused-parameter
LFLAGS = -lrt -lm -lpthread

incr.o: incr.c
	$(CC) $(CFLAGS) -o incr.o incr.c $(LFLAGS)

mmt.o: mmt.c
	$(CC) $(CFLAGS) -o mmt.o mmt.c $(LFLAGS)

.PHONY: clean
clean:
	rm -f *.o
