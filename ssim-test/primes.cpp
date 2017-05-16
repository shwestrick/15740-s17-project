#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <cmath>
#include "utils.hpp"
#include "../ssim/pccc.hpp"

long min(long a, long b) {
  return a < b ? a : b;
}

long max(long a, long b) {
  return a > b ? a : b;
}

Memory* memory;

Cell<int>* P;
Cell<long>* N;
Cell<bool>** isPrime;
Cell<long>* offset;
Cell<long>* n;

barrier B;

void initializeFlags(int tid) {
  long NN = N->read(tid);
  long PP = P->read(tid);
  long chunk = max(1, NN / PP);
  long lo = min(NN, tid * chunk);
  long hi = min(NN, lo + chunk);
  for (long i = lo; i < hi; i++) isPrime[i]->write(tid, true);
  memory->barrier(tid);
  synchronize(&B, tid);
}

void* markPrimes(void* arg) {
  int tid = *((int*) arg);

  initializeFlags(tid);

  while (n->read(tid) < N->read(tid)) {
    
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

    long nn = n->read(tid);
    long ooffset = offset->read(tid);
    long PP = P->read(tid);
    
    info("Hello from %d with offset=%ld and n=%ld", tid, ooffset, nn);

    long m = nn - ooffset;
    long chunk = max(1, m / PP);

    long lo = (tid * chunk) % m;
    long hi = min(m, lo + chunk);

    // figure out how many processors all have the same lo,hi
    long sharers = PP / m + (tid % m < PP % m ? 1 : 0);

    /*printf("%d: %ld sharers in region [%ld,%ld)\n",
           tid, sharers, offset+lo, offset+hi); */
    for (long k = ooffset + lo; k < ooffset + hi; k++) {
      if (isPrime[k]->read(tid)) {
        info("Thread %d considering prime %ld", tid, k);
        long fhi = (N->read(tid) + k - 1) / k - 2; // double check this
        long sid = tid / m; // sharer id
        
        long fchunk = max(1, fhi / sharers);
        long myflo = min(fhi, sid * fchunk);
        long myfhi = min(fhi, myflo + fchunk);
        info("%d(%ld): setting region [%ldk,%ldk) for k=%ld\n",
               tid, sid, myflo+2, myfhi+2, k);
        //for (long f = 0; f < fhi; f++) isPrime[(f+2)*k] = false;
        for (long f = myflo; f < myfhi; f++) {
          if ((f+2)*k >= nn) isPrime[(f+2)*k]->write(tid, false);
        }
      }
      else info("Thread %d skipping non-prime %ld", tid, k);
    }
    
    memory->barrier(tid);
    synchronize(&B, tid);
    
    if (tid == 0) {
      offset->write(tid, nn);
      n->write(tid, min(N->read(tid), nn * nn));
    }

    memory->barrier(tid);
    synchronize(&B, tid);

  }

  memory->barrier(tid);
  return NULL;
}

void verifyIsPrime(long k) {
  long sqrtk = (long) std::ceil(std::sqrt(k));
  for (long i = 2; i < sqrtk; i++) {
    if (k % i == 0) panic("%ld is divisible by %ld", k, i);
  }
}

int main(int argc, char** argv) {
  int PP = argc > 1 ? atoi(argv[1]) : 1;         // number of concurrent threads
  long NN = argc > 2 ? atol(argv[2]) : 100000000; // size of array


  memory = new Memory(PP);

  P = memory->cell<int>("P"); P->write(0, PP);
  N = memory->cell<long>("N"); N->write(0, NN);

  pthread_t threads[PP];
  int thread_ids[PP];

  // stupid hacky fix; make the array a little too big to handle off-by-one error
  isPrime = new Cell<bool>*[NN+3]; //malloc(N * sizeof(long));
  for (long i = 0; i < NN+3; i++) {
    isPrime[i] = memory->cell<bool>("isPrime[" + std::to_string(i) + "]");
  }
  offset = memory->cell<long>("offset"); offset->write(0, 2);
  n = memory->cell<long>("n"); n->write(0, 4);

  memory->barrier(0);
  barrier_init(&B, PP);

  info("Spawning threads");

  // Spawn threads
  for (int t = PP-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &markPrimes, thread_ids + t);
    else markPrimes(thread_ids + t);
  }

  // Cleanup
  // printf("Cleanup...\n");
  for (int t = 1; t < PP; t++) pthread_join(threads[t], NULL);
  info("Joined threads");

  for (long k = 2; k < NN; k++) {
    if (isPrime[k]->read(0)) {
      verifyIsPrime(k);
    }
  }
  delete[] isPrime;
  delete memory;
  // printf("Done.\n");

  return 0;
}
