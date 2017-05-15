#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "utils.h"

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

barrier B;

void advance() {
  offset = n;
  n = min(N, n * n);
}

void initializeFlags(int tid) {
  long chunk = max(1, N / P);
  long lo = min(N, tid * chunk);
  long hi = min(N, lo + chunk);
  for (long i = lo; i < hi; i++) isPrime[i] = true;
  synchronize(&B, tid);
}

void* markPrimes(void* arg) {
  int tid = *((int*) arg);

  initializeFlags(tid);

  while (n < N) {
    
    /* This section is attempting to statically schedule a nested
     * parallel-for loop of the form
     *
     *   pfor {k | offset <= k < n}
     *     pfor {f | 2 <= f < N / k}
     *       isPrime[f*k] := false;
     *
     * You could imagine there are two cases:
     *
     *   if (n - offset) >= P, then each processor can be assigned a unique
     *   region of values of k. The inner loops will then be entirely
     *   sequential.
     *
     *   if (n - offset) < P, then multiple processors need to be assigned to
     *   the same value of k, and the inner loop will also need to be
     *   parallelized. For each k, this is accomplished by forcing processors p
     *   where p % (n - offset) == (k - offset) to share the inner loop for k.
     *   We calculate the number of sharers, and reassign processor ids with
     *   p' := p / (n - offset). The ids {p'} are then used to statically
     *   partition the inner loop.
     *
     * TODO: there appears to be an off-by-one error somewhere. For example,
     * running `primes 8 100` omits the prime 97.
     */

    long m = n - offset;
    long chunk = max(1, m / P);

    long lo = (tid * chunk) % m;
    long hi = min(m, lo + chunk);

    // figure out how many processors all have the same lo,hi
    long sharers = P / m + (tid % m < P % m ? 1 : 0);

    /*printf("%d: %ld sharers in region [%ld,%ld)\n",
           tid, sharers, offset+lo, offset+hi); */
    for (long k = offset + lo; k < offset + hi; k++) {
      if (isPrime[k]) {
        long fhi = N / k - 1; // double check this
        long sid = tid / m; // sharer id
        
        long fchunk = max(1, fhi / sharers);
        long myflo = min(fhi, sid * fchunk);
        long myfhi = min(fhi, myflo + fchunk);
        /*printf("%d(%ld): setting region [%ldk,%ldk) for k=%ld\n",
               tid, sid, myflo+2, myfhi+2, k); */
        //for (long f = 0; f < fhi; f++) isPrime[(f+2)*k] = false;
        for (long f = myflo; f < myfhi; f++) isPrime[(f+2)*k] = false;
      }
    }
    
    synchronize_before(advance, &B, tid);
  }

  return NULL;
}

int main(int argc, char** argv) {
  P = argc > 1 ? atoi(argv[1]) : 1;         // number of concurrent threads
  N = argc > 2 ? atol(argv[2]) : 100000000; // size of array

  barrier_init(&B, P);

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
  //for (long k = 2; k < N; k++) if (isPrime[k]) printf("%ld ", k);
  free(isPrime);
  // printf("Done.\n");

  return 0;
}
