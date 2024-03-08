#include "communicator.h"

namespace tsndgm {

Communicator::Communicator() : prev_best(std::numeric_limits<Delay>::max()) {
  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Op_create(reduce_delay_pair, true, &op);

  coms++;
}

Communicator::Communicator(const Communicator &other)
    : prev_best(std::numeric_limits<Delay>::max()) {
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Op_create(reduce_delay_pair, true, &op);
  coms++;
}

Communicator::~Communicator() {
  coms--;
  if (coms == 0) {
    MPI_Finalize();
  }
}

Communicator::State Communicator::sync(State final, double ratio) {
  Communicator::State state = Communicator::running;
  while (state == Communicator::running)
    state = exchange_state(final, ratio);
  return state;
}

void Communicator::reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                     MPI_Datatype *datatype) {
  assert((*len == 2));

  Delay *in = (Delay *)invec;
  Delay *out = (Delay *)inoutvec;

  if (in[0] < out[0]) {
    out[0] = in[0];
    out[1] = in[1];
  }
}

Communicator::State Communicator::exchange_state(State state, double ratio) {
  assert((0 <= ratio && ratio <= 1));

  int value = 0;
  switch (state) {
  case running:
    value = 0;
    break;
  case found_better:
    value = 2 * size;
    break;
  case terminated:
    value = 1;
    break;
  }

  int sum;
  MPI_Allreduce(&value, &sum, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  if (sum > size)
    return found_better;
  else if (sum >= size * ratio)
    return terminated;
  return running;
}

void Communicator::exchange_best_selection(DisjunctiveGraphModel &dgm,
                                           EncodedSelection &best) {
  if (size == 1) {
    return;
  }

  // get best selection objective of every MPI process
  local = {best.objective, rank};
  MPI_Allreduce(local.data(), global.data(), 2, MPI_LONG, op, MPI_COMM_WORLD);

  auto &buf = best.buf;
  unsigned int buf_size;
  if (global[1] == rank) {
    // broadcast selection to every other MPI process
    buf_size = buf.size();
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
  } else {
    // receive selection and commit it as best known solution
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, global[1], MPI_COMM_WORLD);
    buf.resize(buf_size);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, global[1], MPI_COMM_WORLD);
  }
}

void Communicator::sync_storage(SelectionStorage &storage) {
  if (size == 1)
    return;

  unsigned int buf_size;
  for (int i = 0; i < size; i++) {
    if (i == rank) {
      if (storage.size() == 0 ||
          prev_best <= storage.encoded_best_selections[0].objective) {
        Delay skip = std::numeric_limits<Delay>::max();
        MPI_Bcast(&skip, 1, MPI_LONG, rank, MPI_COMM_WORLD);
        continue;
      }
      EncodedSelection &selection = storage.encoded_best_selections[0];
      MPI_Bcast(&selection.objective, 1, MPI_LONG, rank, MPI_COMM_WORLD);
      buf_size = selection.buf.size();
      MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
      MPI_Bcast(selection.buf.data(), buf_size, MPI_UNSIGNED, rank,
                MPI_COMM_WORLD);
      prev_best = std::min(prev_best, selection.objective);
    } else {
      Delay objective;
      MPI_Bcast(&objective, 1, MPI_LONG, i, MPI_COMM_WORLD);
      if (objective == std::numeric_limits<Delay>::max())
        continue;
      MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, i, MPI_COMM_WORLD);
      std::vector<unsigned int> buf(buf_size);
      MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, i, MPI_COMM_WORLD);
      storage.update_candidates(EncodedSelection(objective, std::move(buf)));
    }
  }
}

} // namespace tsndgm
