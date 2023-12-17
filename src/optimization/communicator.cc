#include "communicator.h"

namespace tsndgm {
void Communicator::reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                     MPI_Datatype *datatype) {
  if (*len != 3)
    MPI_Abort(MPI_COMM_WORLD, 1);

  Delay *in = (Delay *)invec;
  Delay *out = (Delay *)inoutvec;

  if (in[0] < out[0] || (in[0] == out[0] && in[1] < out[1])) {
    for (int i = 0; i < 3; i++)
      out[i] = in[i];
  }
}

void Communicator::exchange_best_selection(DisjunctiveGraphModel &dgm,
                                           BestSelection &best_selection) {
  if (size == 1)
    return;

  // get best selection objective of every MPI process
  local = {best_selection.objective, best_selection.secondary_objective, rank};
  MPI_Allreduce(local.data(), global.data(), 3, MPI_LONG, op, MPI_COMM_WORLD);

  // skip, if global optimum is same as last time
  if (global[0] == old_global[0] && global[1] == old_global[1])
    return;
  old_global = global;

  unsigned int buf_size;
  std::vector<unsigned int> buf;
  if (global[2] == rank) {
    // broadcast selection to every other MPI process
    dgm.restore_commit(best_selection.commit_index, false);
    dgm.encode(buf);
    buf_size = buf.size();
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
  } else {
    // receive selection and commit it as best known solution
    dgm.restore_commit(best_selection.commit_index, false);
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, global[2], MPI_COMM_WORLD);
    buf.resize(buf_size);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, global[2], MPI_COMM_WORLD);
    dgm.decode(buf);

    std::cout << " -> New Best Selection: " << global[0] << " " << global[1]
              << " found by rank " << global[2] << std::endl;

    best_selection = {best_selection.commit_index, global[0], global[1]};
    dgm.commit_all(best_selection.commit_index);
    best_selection.committed = true;
  }
}

} // namespace tsndgm
