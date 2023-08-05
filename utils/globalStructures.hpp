#pragma once

#include <chrono>

#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

std::mutex stateMutex;

std::mutex requestCounterMutex;

std::mutex unhandledRequestsMutex;

std::mutex timeEventsMutex;

struct HandlerState
{
  int lambdas;
  int lambdasRunning;
};

struct PendingRequest
{
  int tag;
  std::function<void(MPI_Request *, char *, int)> callback;
  trantor::EventLoop *loop;
};

struct UnhandledRequest
{
  std::string lambdaId;
  int requestNumber;
};

struct RequestDuration
{
  std::string lambdaId;
  int requestNumber;
  std::chrono::_V2::system_clock::time_point startTime;
  std::chrono::_V2::system_clock::time_point endTime;
  int duration;
};

struct RequestTimeEvent
{
  int requestNumber;
  std::chrono::_V2::system_clock::time_point time;
  std::string event;
};
