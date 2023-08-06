#include "globalStructures.hpp"
#include "logger.hpp"

void mpiHandler(
    int worldSize,
    std::vector<HandlerState> &handlerStates,
    int &requestCounter,
    std::map<int, PendingRequest> &pendingRequests,
    std::queue<UnhandledRequest> &unhandledRequests,
    std::vector<RequestTimeEvent> &timeEvents)
{
  info("MPI Handler started.");
  while (true)
  {
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    info("Received MPI message from {} with request number: {}", status.MPI_SOURCE, status.MPI_TAG);

    if (status.MPI_TAG == 0)
    {
      info("Received exit message from {}", status.MPI_SOURCE);
      break;
    }

    auto mpiProbe = std::chrono::high_resolution_clock::now();
    {
      std::lock_guard<std::mutex> lock(timeEventsMutex);
      timeEvents.push_back({status.MPI_TAG, mpiProbe, "SERVER_MPI_PROBE"});
    }

    {
      std::unique_lock<std::mutex> lock(stateMutex);
      std::unique_lock<std::mutex> lock2(unhandledRequestsMutex);
      if (!unhandledRequests.empty())
      {
        auto unhandledRequest = unhandledRequests.front();
        unhandledRequests.pop();
        info("Request number: {} has been dequed and will be executed on handler {}.", unhandledRequest.requestNumber, status.MPI_SOURCE);
        MPI_Send(unhandledRequest.lambdaId.c_str(), unhandledRequest.lambdaId.size(), MPI_CHAR, status.MPI_SOURCE, unhandledRequest.requestNumber, MPI_COMM_WORLD);
        auto mpiSend = std::chrono::high_resolution_clock::now();
        {
          std::lock_guard<std::mutex> lock(timeEventsMutex);
          timeEvents.push_back({unhandledRequest.requestNumber, mpiSend, "SERVER_MPI_SEND"});
        }
      }
      else
      {
        handlerStates[status.MPI_SOURCE - 1].lambdasRunning--;
      }
    }

    int responseLength;
    MPI_Get_count(&status, MPI_CHAR, &responseLength);

    std::map<int, PendingRequest>::node_type pendingRequest;

    {
      std::lock_guard<std::mutex> lock(pendingRequestsMutex);
      pendingRequest = pendingRequests.extract(status.MPI_TAG);
    }

    if (pendingRequest.empty())
    {
      warn("No pending request found for request number: {}", status.MPI_TAG);
      continue;
    }

    auto value = pendingRequest.mapped();

    char *responseBuffer = new char[responseLength];

    MPI_Request *mpiRequest = new MPI_Request();
    MPI_Irecv(responseBuffer, responseLength, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, mpiRequest);
    auto mpiIrecv = std::chrono::high_resolution_clock::now();
    {
      std::lock_guard<std::mutex> lock(timeEventsMutex);
      timeEvents.push_back({status.MPI_TAG, mpiIrecv, "SERVER_MPI_IRECV"});
    }

    value.loop->queueInLoop([value, mpiRequest, responseBuffer, responseLength]()
                            { value.callback(mpiRequest, responseBuffer, responseLength); });
  }
  info("MPI Handler stopped.");
}
