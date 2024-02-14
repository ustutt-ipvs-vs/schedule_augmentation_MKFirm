#include "communicator.h"

namespace tsndgm {

Communicator::Communicator(bool multithreading)
    : prev_best(std::numeric_limits<Delay>::max()),
      communication_storage(nullptr), sync_semaphore(0), global_state(running),
      multithreading(multithreading) {
  if (multithreading) {
    int provided;
    MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
    assert((provided == MPI_THREAD_MULTIPLE));
  } else {
    MPI_Init(NULL, NULL);
  }
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Op_create(reduce_delay_pair, true, &op);

  std::string filename = "output_rank" + std::to_string(rank) + ".log";
  std::freopen(filename.c_str(), "w", stdout);
}

Communicator::~Communicator() {
  stop_sync_storage();
  MPI_Finalize();
  std::fclose(stdout);
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

void Communicator::stop_sync_storage() {
  global_state = terminated;
  sync_semaphore.release();
  if (multithreading) {
    if (sync_thread.joinable())
      sync_thread.join();
  } else {
    continuous_sync_storage();
  }
}

void Communicator::signal_sync_storage(SelectionStorage &storage) {
  if (multithreading) {
    if (storage.size() > 0) {
      const std::lock_guard<std::mutex> lock(storage_mutex);
      communication_storage.update_candidates(
          storage.encoded_best_selections[0]);
    }
    if (!sync_thread.joinable())
      sync_thread = std::thread(&Communicator::continuous_sync_storage, this);
    else
      sync_semaphore.release();
  } else if (!multithreading) {
    sync_storage();
  }

  const std::lock_guard<std::mutex> lock(storage_mutex);
  for (EncodedSelection &selection :
       communication_storage.encoded_best_selections) {
    storage.update_candidates(std::move(selection));
  }
  communication_storage.encoded_best_selections.clear();
}

void Communicator::continuous_sync_storage() {
  State state;
  do {
    state = exchange_state(global_state, 1);
    if (state != running)
      return;

    sync_storage();

    if (global_state == running) {
      sync_semaphore.acquire();
    }
  } while (state == running);
}

void Communicator::sync_storage() {
  if (size == 1)
    return;

  unsigned int buf_size;
  for (int i = 0; i < size; i++) {
    if (i == rank) {
      const std::lock_guard<std::mutex> lock(storage_mutex);
      if (communication_storage.size() == 0 ||
          prev_best <=
              communication_storage.encoded_best_selections[0].objective) {
        Delay skip = std::numeric_limits<Delay>::max();
        MPI_Bcast(&skip, 1, MPI_LONG, rank, MPI_COMM_WORLD);
        continue;
      }
      EncodedSelection &selection =
          communication_storage.encoded_best_selections[0];
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
      const std::lock_guard<std::mutex> lock(storage_mutex);
      communication_storage.update_candidates(
          EncodedSelection(objective, std::move(buf)));
    }
  }
}

} // namespace tsndgm
