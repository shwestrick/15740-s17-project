#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "concurrency.h"

long min(long a, long b) {
  return a < b ? a : b;
}

long max(long a, long b) {
  return a > b ? a : b;
}

int P;
long N;
bool* isPrime;
long offset;
long n;

void advance() {
  offset = n;
  n = min(N, n * n);
}

void initializeFlags(int tid) {
  long chunk = max(1, N / P);
  long lo = min(N, tid * chunk);
  long hi = min(N, lo + chunk);
  for (long i = lo; i < hi; i++) isPrime[i] = true;
  barrier(tid, P, nop);
}

void* markPrimes(void* arg) {
  int tid = *((int*) arg);

  initializeFlags(tid);

  while (n < N) {
    long m = n - offset;
    long chunk = max(1, m / P);

    long lo = min(m, tid * chunk);
    long hi = min(m, lo + chunk);

    // TODO: parallelize this loop too.
    for (long k = offset + lo; k < offset + hi; k++) {
      if (isPrime[k]) {
        for (long f = 2; f*k < N; f++) isPrime[f*k] = false;
      }
    }
    
    barrier(tid, P, advance);
  }

  return NULL;
}

int main(int argc, char** argv) {
  P = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  N = argc > 2 ? atol(argv[2]) : 100000000; // size of array

  concurrency_init();

  pthread_t threads[P];
  int thread_ids[P];

  isPrime = malloc(N * sizeof(long));
  offset = 2;
  n = 4;

  // Spawn threads
  for (int t = P-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &markPrimes, thread_ids + t);
    else markPrimes(thread_ids + t);
  }

  // Cleanup
  // printf("Cleanup...\n");
  for (int t = 1; t < P; t++) pthread_join(threads[t], NULL);
  // for (long k = 2; k < N; k++) if (isPrime[k]) printf("%ld ", k);
  free(isPrime);
  // printf("Done.\n");

  return 0;
}
