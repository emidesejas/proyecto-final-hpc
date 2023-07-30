#include "globalStructures.hpp"

void mpiHandler(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests, std::queue<UnhandledRequest> &unhandledRequests) {
  LOG_INFO << "MPI Handler started";
  while (true) {
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    LOG_INFO << "MPI Handler received from " << status.MPI_SOURCE << " with tag " << status.MPI_TAG;

    {
      std::unique_lock<std::mutex> lock(stateMutex);
      std::unique_lock<std::mutex> lock2(unhandledRequestsMutex);
      if (!unhandledRequests.empty()) {
        auto unhandledRequest = unhandledRequests.front();
        unhandledRequests.pop();
        LOG_INFO << "Request " << unhandledRequest.requestNumber << " has been dequed and will be executed. lambda id: " << unhandledRequest.lambdaId;
        MPI_Send(unhandledRequest.lambdaId.c_str(), unhandledRequest.lambdaId.size(), MPI_CHAR, status.MPI_SOURCE, unhandledRequest.requestNumber, MPI_COMM_WORLD);
      } else {
        handlerStates[status.MPI_SOURCE].lambdasRunning--;
      }
    }

    int number_amount;
    MPI_Get_count(&status, MPI_CHAR, &number_amount);

    char response[number_amount];
    MPI_Recv(&response, number_amount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

    auto request = pendingRequests.extract(status.MPI_TAG);

    if (request.empty()) {
      LOG_WARN << "No pending request found for tag: " << status.MPI_TAG;
      return;
    }

    auto value = request.mapped();

    std::string stringResponse(response, number_amount);

    value.loop->queueInLoop([value, status, stringResponse, number_amount]() mutable {
      value.callback(status, stringResponse);
    });
  }
}
