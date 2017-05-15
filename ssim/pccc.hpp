#include <pthread.h>
#include <unordered_set>
#include <vector>
#include <stdint.h>

#ifndef _SSIM_PCCC_H_
#define _SSIM_PCCC_H_

template <class T> class Cell;

// ================================== Memory ==================================

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

  Memory(int P);

  template <class T>
  Cell<T>* cell();
  void touch(int id, Entry* e);      // make sure e is in id's l1 cache
  void invalidate(int id, Entry* e); // remove e from id's l1 cache
  void barrier(int id);

};

// =================================== Cell ===================================

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

  Cell(Memory* c);
  T read(int id);
  void write(int id, T v);
  void barrier(int id) override;

};

#endif
