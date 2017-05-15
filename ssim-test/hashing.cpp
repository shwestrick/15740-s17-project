#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include "utils.hpp"
#include "../ssim/pccc.hpp"

Memory* memory;

typedef struct {
  Cell<bool>* empty;
  Cell<long>* value;
  Cell<int>* winner;
} option;

Cell<int>* P;
Cell<long>* N;
Cell<long>* M;
option* T;

Cell<int>* R;
Cell<bool>* again;

barrier B;

inline long offset(int tid, int offset, long n) {
  long k = offset * (n / P->read(tid));
  return (k < n ? k : n);
}

inline void initializeTableEmpty(int tid, long lo, long hi) {
  for (long i = lo; i < hi; i++) {
    T[i].empty->write(tid, true);
    T[i].winner->write(tid, -1);
  }
}

inline long constrain(int tid, long h) {
  long MM = M->read(tid);
  long g = h % MM;
  long i = (g < 0 ? MM - g : g);
  if (i < 0 || i >= MM) printf("Error index %ld\n", i);
  return i;
}

void* threadFunc(void* arg) {
  int tid = *((int*) arg);

  //info("Hello from %d", tid);

  // initialize local data
  long NN = N->read(tid);

  //info("Thread %d calculating offsets", tid);
  long lo = offset(tid, tid, NN);
  long n = offset(tid, tid+1, NN) - lo;
  Cell<long>** D = new Cell<long>*[n]; //malloc(n * sizeof(long));
  for (long i = 0; i < n; i++) {
    //info("Thread %d initializating D[%ld]", tid, i);
    D[i] = memory->cell<long>(std::to_string(tid) + ":D[" + std::to_string(i) + "]");
    D[i]->write(tid, lo+i);
  }

  info("Thread %d Initializing table", tid);

  // initialize a region of the hash table
  initializeTableEmpty(tid, offset(tid, tid, M->read(tid)), offset(tid, tid+1, M->read(tid)));

  memory->barrier(tid);
  synchronize(&B, tid);

  while (again->read(tid)) {

    int r = R->read(tid);

    //printf("==== Round %d ====\n", R);
    info("Thread %d: round %d with %ld items left", tid, r, n);

    for (long i = 0; i < n; i++) {
      long x = D[i]->read(tid);
      //printf("Trying %ld at %ld\n", x, constrain(hashl(x) + r));
      option* elem = T + constrain(tid, hashl(x) + r);
      elem->winner->write(tid, tid);
      // if (elem->empty->read(tid)) {
      //   elem->empty->write(tid, false);
      //   elem->value->write(tid, x);
      // }
    }

    memory->barrier(tid);
    synchronize(&B, tid);

    again->write(tid, false);

    memory->barrier(tid);
    synchronize(&B, tid);

    long j = 0;
    for (long i = 0; i < n; i++) {
      long x = D[i]->read(tid);
      option* elem = T + constrain(tid, hashl(x) + r);
      if (elem->winner->read(tid) == tid && elem->empty->read(tid)) {
        elem->empty->write(tid, false);
        elem->value->write(tid, x);
      }
      else {
        D[j]->write(tid, x);
        j++;
      }
      // if (T[constrain(tid, hashl(x) + r)].value->read(tid) != x) {
      //   //printf("Saving %ld for next round\n", x);
      //   D[j]->write(tid, x);
      //   j++;
      // }
    }

    n = j;
    if (n > 0) again->write(tid, true);
    R->write(tid, r+1);

    memory->barrier(tid);
    synchronize(&B, tid);

  }

  delete[] D;

  memory->barrier(tid);
  return NULL;
}

int main(int argc, char** argv) {
  int PP = argc > 1 ? atoi(argv[1]) : 4;         // number of concurrent threads
  int NN = argc > 2 ? atol(argv[2]) : 100000000; // number of elements to insert
  int MM = 2 * NN; // size of hash table

  memory = new Memory(PP);
  P = memory->cell<int>("P"); P->write(0, PP);
  N = memory->cell<long>("N"); N->write(0, NN);
  M = memory->cell<long>("M"); M->write(0, MM);

  again = memory->cell<bool>("again"); again->write(0, true);
  R = memory->cell<int>("R"); R->write(0, 0);
  T = new option[MM]; //malloc(M * sizeof(option));
  for (long i = 0; i < MM; i++) {
    T[i].empty = memory->cell<bool>("T[" + std::to_string(i) + "].empty");
    T[i].value = memory->cell<long>("T[" + std::to_string(i) + "].value");
    T[i].winner = memory->cell<int>("T[" + std::to_string(i) + "].winner");
  }

  memory->barrier(0);
  barrier_init(&B, PP);

  pthread_t threads[PP];
  int thread_ids[PP];

  info("Spawning threads");

  // Spawn threads
  for (int t = PP-1; t >= 0; t--) {
    thread_ids[t] = t;
    if (t > 0) pthread_create(threads + t, NULL, &threadFunc, thread_ids + t);
    else threadFunc(thread_ids + t);
  }

  for (int t = 1; t < PP; t++) pthread_join(threads[t], NULL);

  info("Joined threads");

  for (long i = 0; i < MM; i++) {
    if (T[i].empty->read(0)) printf("_ ");
    else printf("%ld ", T[i].value->read(0));
  }

  delete[] T;
  delete memory;

  return 0;
}
