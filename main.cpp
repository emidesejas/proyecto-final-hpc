#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

#include "utils/globalStructures.hpp"
#include "utils/misc.hpp"
#include "utils/mpiUtils.hpp"
#include "utils/restServer.hpp"

using namespace drogon;

int main() {
  // Initialize the MPI multithreaded environment
  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);

  LOG_WARN_IF(provided < MPI_THREAD_MULTIPLE) << "MPI does not provide the required threading support";

  int worldSize;
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

  int worldRank;
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  // Print off a hello world message
  printf("%s: Rest Server with rank %d out of %d processors\n",
          processor_name, worldRank, worldSize);

  int availableLambdas[worldSize];

  MPI_Gather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, availableLambdas, 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<HandlerState> handlerStates(worldSize - 1);

  for (int i = 1; i < worldSize; i++)
  {
    handlerStates[i - 1] = { availableLambdas[i], 0 };
  }

  int requestCounter = 0;

  std::map<int, PendingRequest> pendingRequests;

  omp_lock_t my_lock;
  omp_init_lock(&my_lock);

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      restServer(worldSize, handlerStates, requestCounter, pendingRequests);
    }
    #pragma omp section
    {
      mpiHandler(worldSize, handlerStates, requestCounter, pendingRequests);
    }
  }
}
