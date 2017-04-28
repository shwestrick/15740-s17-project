#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "utils.h"

int main(int argc, char** argv) {
  int P = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  int N = argc > 2 ? atoi(argv[2]) : 100000000; // size of array
  
  barrier B;
  barrier_init(&B, P);

  pthread_t threads[P];
  int thread_ids[P];

  long* data = malloc(N * sizeof(long));

  void* threadFunc(void* arg) {
    int tid = *((int*) arg);

    long lo = tid * (N/P);
    long hi = (tid+1) * (N/P);
    if (hi > N) hi = N;

    for (long i = lo; i < hi; i++) data[i] = 0;
    synchronize(&B, tid);
    for (long i = lo; i < hi; i++) data[i] += 1;
    synchronize(&B, tid);
    for (long i = lo; i < hi; i++) data[i] += 1;

    return NULL;
  }

  // Spawn threads
  for (int t = P-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &threadFunc, thread_ids + t);
    else threadFunc(thread_ids + t);
  }

  // Cleanup
  // printf("Cleanup...\n");
  for (int t = 1; t < P; t++) pthread_join(threads[t], NULL);
  // for (int i = 0; i < N; i++) assert(data[i] == 2);
  free(data);
  // printf("Done.\n");

  return 0;
}
