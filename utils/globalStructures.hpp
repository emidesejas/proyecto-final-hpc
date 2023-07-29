#pragma once

#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

std::mutex stateMutex;

std::mutex requestCounterMutex;

struct HandlerState
{
  int lambdas;
  int lambdasRunning;
};

struct PendingRequest
{
  int tag;
  std::function<void (MPI_Status, std::istringstream&)> callback;
  trantor::EventLoop *loop;
};
