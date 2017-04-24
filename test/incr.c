#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

inline int fetchAdd(int* loc, int delta) {
  return __sync_fetch_and_add(loc, delta);
}

inline void atomicIncr(int* loc) {
  fetchAdd(loc, 1);
  return;
}

inline int fetch(int* loc) {
  return fetchAdd(loc, 0);
}

int PHASE_FINISHED = -1;
void phase(int t, int P, void (*f)(void)) {
  if (t == 0) {
    atomicIncr(&PHASE_FINISHED);
    f();
    while (fetch(&PHASE_FINISHED) != P-1);
    PHASE_FINISHED = -1;
  }
  else {
    while (fetch(&PHASE_FINISHED) != 0);
    f();
    atomicIncr(&PHASE_FINISHED);
  }
}

int main(int argc, char** argv) {
  int P = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  int N = argc > 2 ? atoi(argv[2]) : 100000000; // size of array

  pthread_t threads[P];
  int thread_ids[P];

  long* data = malloc(N * sizeof(long));

  // pthread_mutex_t printLock;
  // pthread_mutex_init(&printLock, NULL);

  void* threadFunc(void* arg) {
    int id = *((int*) arg);

    long lo = id * (N/P);
    long hi = (id+1) * (N/P);
    if (hi > N) hi = N;

    // pthread_mutex_lock(&printLock);
    // printf("%d: data[%ld,%ld)\n", id, lo, hi);
    // pthread_mutex_unlock(&printLock);

    void initializeRegion() {
      for (long i = lo; i < hi; i++) {
        data[i] = 0;
      }
    }

    void incrementRegion() {
      for (long i = lo; i < hi; i++) {
        data[i] += 1;
      }
    }

    phase(id, P, initializeRegion);
    phase(id, P, incrementRegion);
    phase(id, P, incrementRegion);

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
