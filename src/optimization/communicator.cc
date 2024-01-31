#include "communicator.h"

namespace tsndgm {

Communicator::Communicator() : prev_best(std::numeric_limits<Delay>::max()) {
  // int provided;
  // MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);
  // assert((provided == MPI_THREAD_MULTIPLE));
  MPI_Init(NULL, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Op_create(reduce_delay_pair, true, &op);

  std::string filename = "output_rank" + std::to_string(rank) + ".log";
  std::freopen(filename.c_str(), "w", stdout);
}

Communicator::~Communicator() {
  if (storage_sync.joinable())
    storage_sync.join();
  MPI::Finalize();
  std::fclose(stdout);
}

Communicator::State Communicator::sync() {
  Communicator::State state = Communicator::running;
  while (state == Communicator::running)
    state = exchange_state(Communicator::terminated);
  return state;
}

void Communicator::reduce_delay_pair(void *invec, void *inoutvec, int *len,
                                     MPI_Datatype *datatype) {
  if (*len != 2) {
    MPI_Abort(MPI_COMM_WORLD, 1);
  }

  Delay *in = (Delay *)invec;
  Delay *out = (Delay *)inoutvec;

  if (in[0] < out[0]) {
    out[0] = in[0];
    out[1] = in[1];
  }
}

Communicator::State Communicator::exchange_state(State state) {
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
  else if (sum > size / 2)
    return terminated;
  return running;
}

BestSelection
Communicator::exchange_best_selection(DisjunctiveGraphModel &dgm,
                                      BestSelection &best_selection) {
  if (size == 1) {
    dgm.restore_commit(*best_selection.commit_index, false);
    assert((best_selection.objective ==
            dgm.critical_path(CriticalPath::Objective::makespan).objective));
    return best_selection;
  }

  // get best selection objective of every MPI process
  local = {best_selection.objective, rank};
  MPI_Allreduce(local.data(), global.data(), 2, MPI_LONG, op, MPI_COMM_WORLD);

  unsigned int buf_size;
  std::vector<unsigned int> buf;
  if (global[1] == rank) {
    // broadcast selection to every other MPI process
    dgm.restore_commit(*best_selection.commit_index, false);
    assert((best_selection.objective ==
            dgm.critical_path(CriticalPath::Objective::makespan).objective));
    dgm.encode(buf);
    buf_size = buf.size();
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
  } else {
    // receive selection and commit it as best known solution
    MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, global[1], MPI_COMM_WORLD);
    buf.resize(buf_size);
    MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, global[1], MPI_COMM_WORLD);
    dgm.decode(buf);

    assert((global[0] ==
            dgm.critical_path(CriticalPath::Objective::makespan).objective));

    best_selection = {best_selection.commit_index, global[0]};
    dgm.commit_all(*best_selection.commit_index);
    best_selection.committed = true;
  }

  return best_selection;
}

void Communicator::signal_sync_storage(SelectionStorage &storage) {
  // sync_storage(storage);
  if (!storage_sync.joinable()) {
    storage_sync =
        std::thread(&Communicator::sync_storage, this, std::ref(storage));
  }
}

void Communicator::sync_storage(SelectionStorage &storage) {
  unsigned int buf_size;
  std::cout << "SYNC" << std::endl;
  for (int i = 0; i < size; i++) {
    std::cout << rank << " " << i << std::endl;
    if (i == rank) {
      EncodedSelection &selection = storage.encoded_best_selections[0];
      if (prev_best <= selection.objective) {
        Delay skip = std::numeric_limits<Delay>::max();
        MPI_Bcast(&skip, 1, MPI_LONG, rank, MPI_COMM_WORLD);
        continue;
      }
      MPI_Bcast(&selection.objective, 1, MPI_LONG, rank, MPI_COMM_WORLD);
      buf_size = selection.buf.size();
      MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, rank, MPI_COMM_WORLD);
      MPI_Bcast(selection.buf.data(), buf_size, MPI_UNSIGNED, rank,
                MPI_COMM_WORLD);
      prev_best = std::min(prev_best, selection.objective);
    } else {
      Delay objective;
      std::cout << "wait: " << i << " " << std::endl;
      MPI_Bcast(&objective, 1, MPI_LONG, i, MPI_COMM_WORLD);
      std::cout << "recv: " << i << " " << objective << std::endl;
      if (objective == std::numeric_limits<Delay>::max())
        continue;
      MPI_Bcast(&buf_size, 1, MPI_UNSIGNED, i, MPI_COMM_WORLD);
      std::vector<unsigned int> buf(buf_size);
      std::cout << "recv: " << i << " " << buf_size << std::endl;
      MPI_Bcast(buf.data(), buf_size, MPI_UNSIGNED, i, MPI_COMM_WORLD);
      storage.update_candidates(EncodedSelection(objective, std::move(buf)));
      prev_best = std::min(prev_best, objective);
    }
  }
}

} // namespace tsndgm
