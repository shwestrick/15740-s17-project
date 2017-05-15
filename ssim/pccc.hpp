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
  virtual bool barrier(int id) = 0;
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
  Cell<T>* cell(std::string name) {
    pthread_mutex_lock(&lock);

    auto x = new Cell<T>(this, name, false);
    cells.insert(x);

    pthread_mutex_unlock(&lock);
    return x;
  }

  template <class T>
  Cell<T>* logged_cell(std::string name) {
    pthread_mutex_lock(&lock);

    auto x = new Cell<T>(this, name, true);
    cells.insert(x);

    pthread_mutex_unlock(&lock);
    return x;
  }

  // make sure e is in id's l1 cache
  void touch(int id, Entry* e) {
    l1caches[id].insert(e);
  }

  // when processor `id` issues a barrier, we need to tell every element of its
  // cache to handle barrier transitions.
  void barrier(int id) {
    auto l1cache = l1caches[id];
    for (auto itr = l1cache.begin(); itr != l1cache.end(); ) {
      if ((*itr)->barrier(id)) itr = l1caches[id].erase(itr);
      else ++itr;
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

const char* DStateName(DState s) {
  switch(s) {
    case DState::Dirty: return "DState.Dirty";
    case DState::Clean: return "DState.Clean";
    case DState::Winner: return "DState.Winner";
    case DState::Valid: return "DState.Valid";
    case DState::Invalid: return "DState.Invalid";
  }
  return "DStateName Error";
}

enum class PState {
  Dirty,
  Clean,
  Winner,
  Shared,
  Old,
  Loser,
  Invalid
};

const char* PStateName(PState s) {
  switch(s) {
    case PState::Dirty: return "PState.Dirty";
    case PState::Clean: return "PState.Clean";
    case PState::Winner: return "PState.Winner";
    case PState::Shared: return "PState.Shared";
    case PState::Old: return "PState.Old";
    case PState::Loser: return "PState.Loser";
    case PState::Invalid: return "PState.Invalid";
  }
  return "PStateName Error";
}

template <class T>
class Cell : public Entry {

private:

  std::string name;
  bool logged; // set to true if you want to see all sorts of info
  T value;
  int registered; // if dstate is one of Dirty|Clean|Winner, then this is the id of the owning thread
  DState dstate;
  PState* pstates;
  pthread_mutex_t lock;
  Memory* memory; // the global memory which owns this cell

public:

  Cell(Memory* m, std::string str, bool dolog) {
    logged = dolog;
    name = str;
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
  bool barrier(int id) override;

};

template <class T>
T Cell<T>::read(int id) {
  assert(0 <= id && id < memory->num_procs);
  memory->touch(id, this);

  pthread_mutex_lock(&lock);

  if (logged) info("Thread %d reading Cell(%s): [%s] [%s %d]", id, name.c_str(), PStateName(pstates[id]), DStateName(dstate), registered);

  T result = value;

  switch (pstates[id]) {
    case PState::Dirty:
      break;
    case PState::Clean:
      break;
    case PState::Winner:
      panic("thread %d attempted to read Cell(%s) while winner.", id, name.c_str());
      break;
    case PState::Shared:
      break;
    case PState::Old:
      pstates[id] = PState::Shared;
      break;
    case PState::Loser:
      //panic("thread %d attempted to read Cell(%s) while loser.", id, name.c_str());
      //break;
    case PState::Invalid:
      switch (dstate) {
        case DState::Dirty:
          panic("thread %d attempted to acquire Cell(%s) while directory has thread %d registered as dirty.", id, name.c_str(), registered);
          break;
        case DState::Clean:
          assert(pstates[registered] == PState::Clean);
          pstates[registered] = PState::Shared;
          pstates[id] = PState::Shared;
          dstate = DState::Valid;
          registered = -1;
          break;
        case DState::Winner:
          panic("thread %d attempted to acquire Cell(%s) while directory has thread %d registered as winner.", id, name.c_str(), registered);
          break;
        case DState::Valid:
          pstates[id] = PState::Shared;
          break;
        case DState::Invalid:
          //info("thread %d is now registered clean", id);
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
  assert(0 <= id && id < memory->num_procs);
  memory->touch(id, this);
  pthread_mutex_lock(&lock);

  if (logged) info("Thread %d writing Cell(%s): [%s] [%s %d]", id, name.c_str(), PStateName(pstates[id]), DStateName(dstate), registered);

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
      //panic("thread %d attempted to write Cell(%s) while shared.", id, name.c_str())
      //assert(dstate == DState::Valid);
      //assert(registered == -1);
      // pstates[id] = PState::Dirty; // FIX
      // dstate = DState::Dirty;
      // registered = id;
      // value = v;
      // break;
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
          //pstates[registered] = PState::Loser;
          pstates[registered] = PState::Invalid; // FIX
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
      // NOTE: It turns out that the directory could be Valid right now
      // assert_msg(dstate == DState::Winner, "Thread %d failed \"dstate == DState::Winner\" during write to Cell(%s), where dstate == %s", id, name.c_str(), DStateName(dstate));
      // assert(registered != id);
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
          //pstates[registered] = PState::Loser;
          pstates[registered] = PState::Invalid; // FIX
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
bool Cell<T>::barrier(int id) {
  //info("Thread %d barrier on Cell(%s)", id, name.c_str());

  assert(0 <= id && id < memory->num_procs);

  bool result = false;

  pthread_mutex_lock(&lock);

  if (logged) info("Thread %d barrier on Cell(%s): [%s] [%s %d]", id, name.c_str(), PStateName(pstates[id]), DStateName(dstate), registered);

  switch (pstates[id]) {
    case PState::Dirty:
      assert_msg(dstate == DState::Dirty && registered == id, "Thread %d failed 'dstate == DState::Dirty && registered == id' during barrier on Cell(%s), where dstate == %s, registered == %d", id, name.c_str(), DStateName(dstate), registered);
      pstates[id] = PState::Clean;
      dstate = DState::Clean;
      break;
    case PState::Clean:
      assert_msg(dstate == DState::Clean && registered == id, "Thread %d failed 'dstate == DState::Clean && registered == id' during barrier on Cell(%s), where dstate == %s, registered == %d", id, name.c_str(), DStateName(dstate), registered);
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
      //assert_msg(dstate == DState::Valid, "Thread %d failed \"dstate == DState::Valid\", where dstate == %s", id, DStateName(dstate));
      //assert(registered == -1);
      pstates[id] = PState::Old;
      break;
    case PState::Old:
    case PState::Loser:
    case PState::Invalid:
      pstates[id] = PState::Invalid;
      result = false;
      break;
  }

  pthread_mutex_unlock(&lock);
  return result;
}

#endif
