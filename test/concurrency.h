inline int atomicAdd(int* loc, int delta) {
  return __sync_add_and_fetch(loc, delta);
}

inline int fetch(int* loc) {
  return atomicAdd(loc, 0);
}

void nop() { return; }

// TODO: better barrier

int foo[65];
int* IN_BARRIER = foo;
int* SENSE = foo + 64;

void concurrency_init() {
  *IN_BARRIER = 0;
  *SENSE = 0;
}

void barrier(int tid, int P, void (*f)(void)) {
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
