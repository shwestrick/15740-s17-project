#include <pthread.h>
#include <unordered_set>
#include <vector>
#include <stdint.h>
#include "utils.h"

#ifndef _SSIM_PCCC_H_
#define _SSIM_PCCC_H_

template <class T> class Cell; // forward declaration

/* ========================================================================= *
 * ============================ Global Memory ============================== *
 * ========================================================================= */

class Entry {
public:
  virtual void barrier(int id) = 0;
};

class Memory {

private:

  std::vector<std::unordered_set<Entry*>> l1caches;

  pthread_mutex_t lock;
  std::unordered_set<Entry*> directory;
  std::unordered_set<Entry*> cells;

public:

  int num_procs;

  Memory(int P) {
    pthread_mutex_init(&lock, NULL);
    num_procs = P;
    for (int p = 0; p < P; p++) {
      l1caches.push_back(std::unordered_set<Entry*>());
    }
  }

  template <class T>
  Cell<T>* cell() {
    pthread_mutex_lock(&lock);

    auto x = new Cell<T>(this);
    cells.insert(x);

    pthread_mutex_unlock(&lock);
    return x;
  }

  // make sure e is in id's l1 cache
  void touch(int id, Entry* e) {
    l1caches[id].insert(e);
  }

  // remove e from id's l1 cache
  void invalidate(int id, Entry* e) {
    l1caches[id].erase(e);
  }

  // when processor `id` issues a barrier, we need to tell every element of its
  // cache to handle barrier transitions.
  void barrier(int id) {
    auto l1cache = l1caches[id];
    for (auto itr = l1cache.begin(); itr != l1cache.end(); ++itr) {
      (*itr)->barrier(id);
    }
  }

};

/* ========================================================================= *
 * ======================== Individual Memory Cells ======================== *
 * ========================================================================= */

enum class DState {
  Dirty,
  Clean,
  Winner,
  Valid,
  Invalid
};

enum class PState {
  Dirty,
  Clean,
  Winner,
  Shared,
  Old,
  Loser,
  Invalid
};

template <class T>
class Cell : public Entry {

private:

  T value;
  int registered; // if dstate is one of Dirty|Clean|Winner, then this is the id of the owning thread
  DState dstate;
  PState* pstates;
  pthread_mutex_t lock;
  Memory* memory; // the global memory which owns this cell

public:

  Cell(Memory* m) {
    memory = m;
    registered = -1;
    dstate = DState::Invalid;
    pstates = new PState[m->num_procs];
    for (int p = 0; p < m->num_procs; p++) {
      pstates[p] = PState::Invalid;
    }
    pthread_mutex_init(&lock, NULL);
  }

  T read(int id);
  void write(int id, T v);
  void barrier(int id) override;

};

template <class T>
T Cell<T>::read(int id) {
  pthread_mutex_lock(&lock);

  T result = value;

  switch (pstates[id]) {
    case PState::Dirty:
      break;
    case PState::Clean:
      break;
    case PState::Winner:
      panic("thread %d attempted to read while winner.", id);
      break;
    case PState::Shared:
      break;
    case PState::Old:
      pstates[id] = PState::Shared;
      break;
    case PState::Loser:
      panic("thread %d attempted to read while loser.", id);
      break;
    case PState::Invalid:
      switch (dstate) {
        case DState::Dirty:
          panic("thread %d attempted to acquire while directory has thread %d registered as dirty.", id, registered);
          break;
        case DState::Clean:
          assert(pstates[registered] == PState::Clean);
          pstates[registered] = PState::Shared;
          pstates[id] = PState::Shared;
          dstate = DState::Valid;
          registered = -1;
          break;
        case DState::Winner:
          panic("thread %d attempted to acquire while directory has thread %d registered as winner.", id, registered);
          break;
        case DState::Valid:
          pstates[id] = PState::Shared;
          break;
        case DState::Invalid:
          info("thread %d is now registered clean", id);
          pstates[id] = PState::Clean;
          dstate = DState::Clean;
          registered = id;
          break;
      }
      break;
  }

  pthread_mutex_unlock(&lock);
  return result;
}

template <class T>
void Cell<T>::write(int id, T v) {
  pthread_mutex_lock(&lock);

  switch (pstates[id]) {
    case PState::Dirty:
      assert(dstate == DState::Dirty);
      assert(registered == id);
      value = v;
      break;
    case PState::Clean:
      assert(dstate == DState::Clean);
      assert(registered == id);
      pstates[id] = PState::Dirty;
      dstate = DState::Dirty;
      value = v;
      break;
    case PState::Winner:
      assert(dstate == DState::Winner);
      assert(registered == id);
      value = v;
      break;
    case PState::Shared:
      panic("thread %d attempted to write while shared.", id)
      break;
    case PState::Old:
      switch (dstate) {
        case DState::Dirty:
          assert(registered != id);
          pstates[id] = PState::Loser;
          pstates[registered] = PState::Winner;
          dstate = DState::Winner;
          break;
        case DState::Clean:
          assert(registered != id);
          pstates[id] = PState::Winner;
          pstates[registered] = PState::Loser;
          dstate = DState::Winner;
          registered = id;
          value = v;
          break;
        case DState::Winner:
          assert(registered != id);
          pstates[id] = PState::Loser;
          break;
        case DState::Valid:
          assert(registered == -1);
          pstates[id] = PState::Dirty;
          dstate = DState::Dirty;
          registered = id;
          value = v;
          break;
        case DState::Invalid:
          panic("non-inclusivity at a write?");
          break;
      }
      break;
    case PState::Loser:
      assert(dstate == DState::Winner);
      assert(registered != id);
      break;
    case PState::Invalid:
      switch (dstate) {
        case DState::Dirty:
          assert(registered != id);
          pstates[id] = PState::Loser;
          pstates[registered] = PState::Winner;
          dstate = DState::Winner;
          break;
        case DState::Clean:
          assert(registered != id);
          pstates[id] = PState::Winner;
          pstates[registered] = PState::Loser;
          dstate = DState::Winner;
          registered = id;
          value = v;
          break;
        case DState::Winner:
          assert(registered != id);
          pstates[id] = PState::Loser;
          break;
        case DState::Valid:
          assert(registered == -1);
          pstates[id] = PState::Dirty;
          dstate = DState::Dirty;
          registered = id;
          value = v;
          break;
        case DState::Invalid:
          assert(registered == -1);
          pstates[id] = PState::Dirty;
          dstate = DState::Dirty;
          registered = id;
          value = v;
          break;
      }
      break;
  }

  pthread_mutex_unlock(&lock);
}

template <class T>
void Cell<T>::barrier(int id) {
  pthread_mutex_lock(&lock);

  switch (pstates[id]) {
    case PState::Dirty:
      assert(dstate == DState::Dirty);
      assert(registered == id);
      pstates[id] = PState::Clean;
      dstate = DState::Clean;
      break;
    case PState::Clean:
      assert(dstate == DState::Clean);
      assert(registered == id);
      break;
    case PState::Winner:
      assert(dstate == DState::Winner);
      assert(registered == id);
      pstates[id] = PState::Old;
      dstate = DState::Valid;
      registered = -1;
      break;
    case PState::Shared:
      assert(dstate == DState::Valid);
      assert(registered == -1);
      pstates[id] = PState::Old;
      break;
    case PState::Old:
    case PState::Loser:
    case PState::Invalid:
      pstates[id] = PState::Invalid;
      memory->invalidate(id, this);
      break;
  }

  pthread_mutex_unlock(&lock);
}

#endif
