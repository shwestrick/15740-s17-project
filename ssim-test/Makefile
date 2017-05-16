CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -Wfatal-errors -Wno-unused-parameter
LFLAGS = -lrt -lm -lpthread

hashing: utils.hpp hashing.cpp ../ssim/pccc.hpp
	$(CC) $(CFLAGS) -o hashing hashing.cpp $(LFLAGS)

primes: utils.hpp primes.cpp ../ssim/pccc.hpp
	$(CC) $(CFLAGS) -o primes primes.cpp $(LFLAGS)

.PHONY: clean
clean:
	rm -f hashing
