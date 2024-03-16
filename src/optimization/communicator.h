#ifndef TSN_DGM_MPI_UTILS_H
#define TSN_DGM_MPI_UTILS_H

#include "../dgm/dgm.h"
#include "selection.h"
#include "selection_storage.h"
#include <cstdio>
#include <iostream>
#include <mpi.h>
#include <semaphore>
#include <thread>

namespace tsndgm {

struct SynchronizationSelection {
  EncodedSelection selection;
  std::mutex m;
};

static int coms = 0;
class Communicator {
public:
  enum State { running, found_better, terminated };

  Communicator(const Communicator &other);
  Communicator();

  State exchange_state(State state, double ratio = 0.5);
  Communicator::State sync(State final = terminated, double ratio = 0.5);

  void
  exchange_best_selection(SynchronizationSelection &sync_selection,
                          Delay prev_best = std::numeric_limits<Delay>::max());
  void continuous_exchange(SynchronizationSelection &sync_selection);

  ~Communicator();

  int rank;
  int size;
  State global_state;

private:
  MPI_Op op;
  std::array<Delay, 2> local, global;

  static void reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                MPI_Datatype *datatype);
};

class Synchronization {
public:
  Synchronization(Communicator &com) : com(com){};
  Synchronization(const Synchronization &other) : com(other.com) {}

  void start();
  void stop(SelectionStorage &storage);
  void update(SelectionStorage &storage);

private:
  Communicator &com;
  SynchronizationSelection sync_selection;

  std::thread sync_thread;
};

} // namespace tsndgm

#endif // TSN_DGM_MPI_UTILS_H
