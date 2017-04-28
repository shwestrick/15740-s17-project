#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "utils.h"

typedef struct {
  bool empty;
  long value;
} option;

int P;
long N;
long M;
option* T;

int R;
bool again;

barrier B;

inline long offset(int tid, long n) {
  long k = tid * (n / P);
  return (k < n ? k : n);
}

inline void initializeTableEmpty(long lo, long hi) {
  for (long i = lo; i < hi; i++)
    T[i].empty = true;
}

inline long constrain(long h) {
  long g = h % M;
  long i = (g < 0 ? M - g : g);
  if (i < 0 || i >= M) printf("Error index %ld\n", i);
  return i;
}

void set_again_false() { again = false; }

void* threadFunc(void* arg) {
  int tid = *((int*) arg);
  
  // initialize local data
  long lo = offset(tid, N);
  long n = offset(tid+1, N) - lo;
  long* D = malloc(n * sizeof(long));
  for (long i = 0; i < n; i++) D[i] = lo+i;

  // initialize a region of the hash table
  initializeTableEmpty(offset(tid, M), offset(tid+1, M));

  synchronize(&B, tid);

  while (again) {

    int r = R;

    //printf("==== Round %d ====\n", R);
    //printf("%d: round %d with %ld items left\n", tid, r, n);
    
    for (long i = 0; i < n; i++) {
      long x = D[i];
      //printf("Trying %ld at %ld\n", x, constrain(hashl(x) + r));
      option* elem = T + constrain(hashl(x) + r);
      if (elem->empty) {
        elem->empty = false;
        elem->value = x;
      }
    }

    synchronize_before(set_again_false, &B, tid);

    long j = 0;
    for (long i = 0; i < n; i++) {
      long x = D[i];
      if (T[constrain(hashl(x) + r)].value != x) {
        //printf("Saving %ld for next round\n", x);
        D[j] = x;
        j++;
      }
    }

    n = j;
    if (n > 0) again = true;
    R = r+1;

    synchronize(&B, tid);

  }

  free(D);

  return NULL; 
}

int main(int argc, char** argv) {
  P = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  N = argc > 2 ? atol(argv[2]) : 100000000; // number of elements to insert
  M = 2 * N; // size of hash table

  again = true;
  R = 0;
  T = malloc(M * sizeof(option));

  barrier_init(&B, P);

  pthread_t threads[P];
  int thread_ids[P];

  // Spawn threads
  for (int t = P-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &threadFunc, thread_ids + t);
    else threadFunc(thread_ids + t);
  }

  for (int t = 1; t < P; t++) pthread_join(threads[t], NULL);
  /*
  for (long i = 0; i < M; i++) {
    if (T[i].empty) printf("_ ");
    else printf("%ld ", T[i].value);
  }
  */
  free(T);

  return 0;
}
