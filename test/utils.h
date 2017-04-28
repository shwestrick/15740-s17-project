#include <math.h>
#include <time.h>

inline int atomicAdd(int* loc, int delta) {
  return __sync_add_and_fetch(loc, delta);
}

inline int fetch(int* loc) {
  return atomicAdd(loc, 0);
}

void nop() { return; }

inline unsigned int hash(unsigned int a) {
  a = (a+0x7ed55d16) + (a<<12);
  a = (a^0xc761c23c) ^ (a>>19);
  a = (a+0x165667b1) + (a<<5);
  a = (a+0xd3a2646c) ^ (a<<9);
  a = (a+0xfd7046c5) + (a<<3);
  a = (a^0xb55a4f09) ^ (a>>16);
  return a;
}

inline unsigned long hashl(unsigned long x) {
  unsigned int l = x;
  unsigned int u = x >> (8 * sizeof(int));
  return (((unsigned long) hash(u)) << (8 * sizeof(int)))
       | ((unsigned long) hash(l));
}

inline long random_range(long seed, long lo, long hi) {
  if (hi <= lo) hi = lo+1;
  return lo + (hashl(seed) % (hi - lo));
}

// TODO: better barrier

typedef struct {
  int seed;
  int procs;
  int padding[65];
} barrier;

inline int* count(barrier* b) {
  return b->padding;
}

inline int* sense(barrier* b) {
  return b->padding + 64;
}

void barrier_init(barrier* b, int P) {
  b->seed = 42;
  b->procs = P;
  *count(b) = 0;
  *sense(b) = 0;
}

inline long long_min(long a, long b) {
  return a < b ? a : b;
}

void blink(long ns) {
  struct timespec t;
  t.tv_sec = 0;
  t.tv_nsec = ns; 
  nanosleep(&t, NULL);
}  

void synchronize_before(void (*f)(void), barrier* b, int tid) {
  int seed = b->seed;
  int s = (fetch(sense(b)) == 0 ? 1 : 0);
  if (atomicAdd(count(b), 1) != b->procs) {
    int tries = 0;
    int w = ceil(log(b->procs)/log(2));
    long myseed = (tid << (sizeof(long)-w)) | seed;
    while (fetch(sense(b)) != s) {
      blink(random_range(myseed, 0, long_min(10000000, 2 << tries))); 
      tries++;
      myseed++;
    }
  }
  else {
    f();
    b->seed += 1;
    *count(b) = 0;
    atomicAdd(sense(b), (s == 1 ? 1 : -1));
  }
}

void synchronize(barrier* b, int tid) {
  synchronize_before(nop, b, tid);
}
