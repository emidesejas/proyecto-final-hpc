#pragma once

#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

std::mutex stateMutex;

std::mutex requestCounterMutex;

std::mutex unhandledRequestsMutex;

struct HandlerState {
  int lambdas;
  int lambdasRunning;
};

struct PendingRequest {
  int tag;
  std::function<void (MPI_Status, std::string)> callback;
  trantor::EventLoop *loop;
};

struct UnhandledRequest {
  std::string lambdaId;
  int requestNumber;
};
