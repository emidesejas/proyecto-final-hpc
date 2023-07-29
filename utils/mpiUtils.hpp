#include "globalStructures.hpp"

void mpiHandler(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests) {
  LOG_INFO << "MPI Handler started";
  while (true) {
    MPI_Status status;
    MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    LOG_INFO << "MPI Handler received from " << status.MPI_SOURCE << " with tag " << status.MPI_TAG;

    int number_amount;
    MPI_Get_count(&status, MPI_CHAR, &number_amount);

    char response[number_amount];
    
    MPI_Recv(&response, number_amount, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);

    auto request = pendingRequests.extract(status.MPI_TAG);

    if (request.empty()) {
      LOG_INFO << "No pending request found for tag: " << status.MPI_TAG;
      return;
    }

    auto value = request.mapped();

    LOG_INFO << "Will enqueue callback for tag: " << status.MPI_TAG;

    std::istringstream stream(response);

    value.loop->queueInLoop([value, status, &stream, number_amount]() mutable {
      value.callback(status, stream);
    });    
  }
}