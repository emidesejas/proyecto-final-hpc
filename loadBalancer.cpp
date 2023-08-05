#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

#include "utils/globalStructures.hpp"
#include "utils/misc.hpp"
#include "utils/mpiUtils.hpp"
#include "utils/restServer.hpp"
#include "utils/logger.hpp"

using namespace drogon;

int main()
{
  char processorName[MPI_MAX_PROCESSOR_NAME];
  int processorNameLength;

  // Initialize the MPI multithreaded environment
  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);

  MPI_Get_processor_name(processorName, &processorNameLength);
  auto deviceName = "Load Balancer " + std::string(processorName);
  console::internal::setDeviceString(deviceName);

  if (provided < MPI_THREAD_MULTIPLE)
  {
    warn("MPI thread level below required");
  }
  else
  {
    info("MPI thread level is sufficient");
  }

  int worldSize;
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

  info("Nodes on the network: {}", worldSize);

  int worldRank;
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  if (worldRank != 0)
  {
    error("Load Balancer must be started on rank 0");
    MPI_Finalize();
    return 1;
  }

  info("Starting Load Balancer with rank: {}", worldRank);

  int availableLambdas[worldSize];
  MPI_Gather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, availableLambdas, 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<HandlerState> handlerStates(worldSize - 1);
  for (int i = 1; i < worldSize; i++)
  {
    handlerStates[i - 1] = {availableLambdas[i], 0};
  }

  int requestCounter = 0;

  std::map<int, PendingRequest> pendingRequests;
  std::queue<UnhandledRequest> unhandledRequests;

  std::map<int, RequestDuration> durations;
  std::vector<RequestTimeEvent> timeEvents;

#pragma omp parallel sections num_threads(2)
  {
#pragma omp section
    {
      console::internal::setDeviceString(deviceName);
      restServer(worldSize, handlerStates, requestCounter, pendingRequests, unhandledRequests, durations, timeEvents);
    }
#pragma omp section
    {
      console::internal::setDeviceString(deviceName);
      mpiHandler(worldSize, handlerStates, requestCounter, pendingRequests, unhandledRequests, timeEvents);
    }
  }

  exit(EXIT_SUCCESS);
}
