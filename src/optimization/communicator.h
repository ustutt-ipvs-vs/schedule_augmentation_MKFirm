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

class Communicator {
public:
  enum State { running, found_better, terminated };

  Communicator(bool multithreading = true);
  BestSelection exchange_best_selection(DisjunctiveGraphModel &dgm,
                                        BestSelection &best_selection);
  void sync_storage(SelectionStorage &storage);
  State exchange_state(State state, double ratio = 0.5);

  Communicator::State sync(State final = terminated, double ratio = 0.5);
  void signal_sync_storage(SelectionStorage &storage);
  void stop_sync_storage();

  ~Communicator();

  template <typename T> std::pair<T, T> partition(T first, T last) {
    size_t n = last - first;
    size_t q = n / size, r = n % size;
    size_t offset = rank * q + (rank < r ? rank : r);
    size_t len = q + (rank < r ? 1 : 0);
    return {first + offset, first + offset + len};
  }

  template <typename T> bool smaller_partition(T first, T last) {
    size_t r = (last - first) % size;
    return rank >= r;
  }

  int rank;
  int size;
  std::thread storage_sync;

private:
  MPI_Op op;
  std::array<Delay, 2> local, global;

  Delay prev_best;
  bool multithreading;
  std::binary_semaphore sync_semaphore;
  State global_state;

  static void reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                MPI_Datatype *datatype);
};

} // namespace tsndgm

#endif // TSN_DGM_MPI_UTILS_H
