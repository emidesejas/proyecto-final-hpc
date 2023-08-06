#pragma once

#include <chrono>

#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

inline std::mutex stateMutex;

inline std::mutex requestCounterMutex;

inline std::mutex unhandledRequestsMutex;

inline std::mutex timeEventsMutex;

inline std::mutex pendingRequestsMutex;

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
