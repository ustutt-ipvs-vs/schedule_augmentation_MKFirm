#include "communicator.h"

namespace tsndgm {

Communicator::Communicator() : global_state(running) {
  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &provided);
  if (provided < MPI_THREAD_SERIALIZED)
    throw std::runtime_error("MPI Error: required 2, provided " +
                             std::to_string(provided));

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Op_create(reduce_delay_pair, true, &op);

  coms++;
}

Communicator::Communicator(const Communicator &other) : global_state(running) {
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

void Communicator::exchange_best_selection(
    SynchronizationSelection &sync_selection, Delay prev_best) {
  if (size == 1) {
    return;
  }

  EncodedSelection selection;
  {
    std::lock_guard<std::mutex> lock(sync_selection.m);
    selection = sync_selection.selection;
  }

  // get best selection objective of every MPI process
  local = {selection.objective, rank};
  MPI_Allreduce(local.data(), global.data(), 2, MPI_LONG, op, MPI_COMM_WORLD);

  if (prev_best <= global[0])
    return;

  auto &buf = selection.buf;
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
    selection.objective = global[0];
    {
      std::lock_guard<std::mutex> lock(sync_selection.m);
      sync_selection.selection = selection;
    }
  }
}

void Communicator::continuous_exchange(
    SynchronizationSelection &sync_selection) {
  Delay prev_best = std::numeric_limits<Delay>::max();
  State state = running;
  while ((state = exchange_state(global_state, 1)) == running) {
    exchange_best_selection(sync_selection, prev_best);
    prev_best = std::min(sync_selection.selection.objective, prev_best);
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

void Synchronization::start() {
  if (com.size > 1)
    sync_thread = std::thread(&Communicator::continuous_exchange, &com,
                              std::ref(sync_selection));
}

void Synchronization::stop(SelectionStorage &storage) {
  if (com.size > 1) {
    update(storage);
    com.global_state = Communicator::terminated;
    sync_thread.join();
  }
}

void Synchronization::update(SelectionStorage &storage) {
  if (com.size == 1)
    return;

  if (storage.best().objective < sync_selection.selection.objective) {
    std::lock_guard<std::mutex> lock(sync_selection.m);
    sync_selection.selection = storage.best();
  } else if (storage.best().objective > sync_selection.selection.objective) {
    std::lock_guard<std::mutex> lock(sync_selection.m);
    storage.update_candidates(sync_selection.selection);
  }
}

} // namespace tsndgm
