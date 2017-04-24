#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

long min(long a, long b) {
  return a < b ? a : b;
}

long max(long a, long b) {
  return a > b ? a : b;
}

inline int atomicAdd(int* loc, int delta) {
  return __sync_add_and_fetch(loc, delta);
}

inline int fetch(int* loc) {
  return atomicAdd(loc, 0);
}

int P;
bool* isPrime;
long offset;
long range;
long N;

int foo[65];
int* IN_BARRIER = foo;
int* SENSE = foo + 64;

void barrier(int tid, void (*f)(void)) {
  int s = (fetch(SENSE) == 0 ? 1 : 0);
  if (atomicAdd(IN_BARRIER, 1) != P) {
    while (fetch(SENSE) != s);
  }
  else {
    f();
    atomicAdd(IN_BARRIER, -P);
    atomicAdd(SENSE, (s == 1 ? 1 : -1));
  }
}

void advance() {
  offset = range;
  range = min(N, range * range);
}

void nop() { return; }

//void sieve(long n) {
//  for (long k = 2; k < 

void* markPrimes(void* arg) {
  int tid = *((int*) arg);

  /* initialization phase */
  {
    long chunk = max(1, N / P);
    long lo = min(N, tid * chunk);
    long hi = min(N, lo + chunk);
    for (long i = lo; i < hi; i++) isPrime[i] = true;
    barrier(tid, nop);
  }

  while (range - offset > 0) {
    long m = range - offset;
    long chunk = max(1, m / P);

    long lo = min(m, tid * chunk);
    long hi = min(m, lo + chunk);

    for (long k = offset + lo; k < offset + hi; k++) {
      if (isPrime[k]) for (long f = 2; f*k < N; f++) isPrime[f*k] = false;
    }
    
    barrier(tid, advance);
  }

  return NULL;
}

int main(int argc, char** argv) {
  P = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  N = argc > 2 ? atol(argv[2]) : 100000000; // size of array

  *IN_BARRIER = 0;
  *SENSE = 0;

  pthread_t threads[P];
  int thread_ids[P];

  isPrime = malloc(N * sizeof(long));
  offset = 2;
  range = 4;

  // Spawn threads
  for (int t = P-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &markPrimes, thread_ids + t);
    else markPrimes(thread_ids + t);
  }

  // Cleanup
  // printf("Cleanup...\n");
  for (int t = 1; t < P; t++) pthread_join(threads[t], NULL);
  //for (long k = 2; k < N; k++) if (isPrime[k]) printf("%ld ", k);
  free(isPrime);
  // printf("Done.\n");

  return 0;
}
