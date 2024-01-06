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

int Communicator::exchange_state(int state) {
  int max_state;
  MPI_Allreduce(&state, &max_state, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
  return max_state;
}

BestSelection
Communicator::exchange_best_selection(DisjunctiveGraphModel &dgm,
                                      BestSelection &best_selection) {
  if (size == 1)
    return best_selection;

  // get best selection objective of every MPI process
  local = {best_selection.objective, best_selection.secondary_objective, rank};
  MPI_Allreduce(local.data(), global.data(), 3, MPI_LONG, op, MPI_COMM_WORLD);

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
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, global[2], MPI_COMM_WORLD);
    buf.resize(buf_size);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, global[2], MPI_COMM_WORLD);
    dgm.decode(buf);

    best_selection = {best_selection.commit_index, global[0], global[1]};
    dgm.commit_all(best_selection.commit_index);
    best_selection.committed = true;
  }
  return best_selection;
}

} // namespace tsndgm
