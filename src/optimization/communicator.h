#ifndef TSN_DGM_MPI_UTILS_H
#define TSN_DGM_MPI_UTILS_H

#include "../dgm/dgm.h"
#include "../optimization/selection.h"
#include <cstdio>
#include <iostream>
#include <mpi.h>

namespace tsndgm {

class Communicator {
public:
  Communicator() {
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Op_create(reduce_delay_pair, true, &op);

    std::string filename = "output_rank" + std::to_string(rank) + ".log";
    std::freopen(filename.c_str(), "w", stdout);
  }

  int exchange_state(int state);
  BestSelection exchange_best_selection(DisjunctiveGraphModel &dgm,
                                        BestSelection &best_selection);

  void barrier() { MPI_Barrier(MPI_COMM_WORLD); };

  ~Communicator() {
    MPI::Finalize();
    std::fclose(stdout);
  }

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

private:
  MPI_Op op;
  std::array<Delay, 3> local, global;

  static void reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                MPI_Datatype *datatype);
};

} // namespace tsndgm

#endif // TSN_DGM_MPI_UTILS_H
