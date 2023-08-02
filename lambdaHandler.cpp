#include <cstdlib>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <mpi.h>

#include <mpi.h>
#include <fmt/format.h>
#include <cxxopts.hpp>
#include <unistd.h>

#include "utils/logger.hpp"

#define READ_END 0
#define WRITE_END 1

struct LambdaData
{
  std::string lambdaId;
  int requestNumber;
};

std::queue<LambdaData> taskQueue;
std::mutex queueMutex;
std::condition_variable queueCond;
std::atomic<bool> doneProbing(false);

int handleLambda(int rank, std::string lambdaId, int requestNumber);

void masterThread()
{
  info("Master thread started");
  while (true)
  {
    MPI_Status status;
    MPI_Probe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    info("Will execute request: {}", status.MPI_TAG);

    int requestLength;
    MPI_Get_count(&status, MPI_CHAR, &requestLength);

    char request[requestLength];
    MPI_Recv(&request, requestLength, MPI_CHAR, 0, status.MPI_TAG, MPI_COMM_WORLD, &status);

    // Convert the char array to std::string
    std::string requestString(request, requestLength);

    // TODO: add logic to terminate the process from the main process
    if (requestString == "0")
    {
      break;
    }

    {
      std::lock_guard<std::mutex> lock(queueMutex);
      taskQueue.push({requestString, status.MPI_TAG});
    }
    queueCond.notify_one();
  }
  doneProbing.store(true);
}

void lambdaWorker(int rank, int threadId, std::string deviceName)
{
  console::internal::setDeviceString(deviceName);
  while (!doneProbing.load() || !taskQueue.empty())
  {
    info("Worker thread {} waiting for task.", threadId);
    LambdaData currentStatus;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      queueCond.wait(lock, []
                     { return !taskQueue.empty(); });

      currentStatus = taskQueue.front();
      taskQueue.pop();
    }
    handleLambda(rank, currentStatus.lambdaId, currentStatus.requestNumber);
  }
}

int main(int argc, char **argv)
{
  cxxopts::Options options("Lambda Handler", "Executes lambda functions");
  options.add_options()("l,lambdas", "How many lambdas can this node handle", cxxopts::value<int>()->default_value("1"));
  auto params = options.parse(argc, argv);
  auto lambdas = params["lambdas"].as<int>();

  // Initialize the MPI multithreaded environment
  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);

  int worldSize;
  MPI_Comm_size(MPI_COMM_WORLD, &worldSize);

  int worldRank;
  MPI_Comm_rank(MPI_COMM_WORLD, &worldRank);

  char processorName[MPI_MAX_PROCESSOR_NAME];
  int processorNameLength;
  MPI_Get_processor_name(processorName, &processorNameLength);
  auto deviceName = "Worker " + std::string(processorName);
  console::internal::setDeviceString(deviceName);

  MPI_Gather(&lambdas, 1, MPI_INT, NULL, 0, MPI_DATATYPE_NULL, 0, MPI_COMM_WORLD);
  info("Handler with rank {}. Handling {} lambdas.", worldRank, lambdas);

  std::vector<std::thread> workerThreads;

  for (int i = 0; i < lambdas; ++i)
  {
    workerThreads.emplace_back(lambdaWorker, worldRank, i, deviceName);
  }

  masterThread();

  for (auto &thread : workerThreads)
  {
    thread.join();
  }

  MPI_Finalize();
}

int handleLambda(int handlerRank, std::string lambdaId, int requestNumber)
{
  // Fork a child process
  pid_t pid = fork();

  auto fifoSuffix = fmt::format("{}_{}", handlerRank, requestNumber);
  auto toNodeFifo = fmt::format("toNode_{}", fifoSuffix);
  auto fromNodeFifo = fmt::format("fromNode_{}", fifoSuffix);

  if (pid > 0)
  { // parent process
    // Create the named pipes
    mkfifo(toNodeFifo.c_str(), 0666);
    mkfifo(fromNodeFifo.c_str(), 0666);

    // Open the toNodeFifo for writing
    int fdOut = open(toNodeFifo.c_str(), O_WRONLY);
    if (fdOut == -1)
    {
      error("Failed to open toNodeFifo for writing");
      return 1;
    }

    // Write to node
    write(fdOut, lambdaId.c_str(), lambdaId.size());

    // Close the write end of the pipe
    close(fdOut);

    // Open the fromNodeFifo for reading
    int fdIn = open(fromNodeFifo.c_str(), O_RDONLY);
    if (fdIn == -1)
    {
      error("Failed to open fromNodeFifo for reading");
      return 1;
    }

    int64_t dataSize;
    int bytesRead = read(fdIn, &dataSize, sizeof(dataSize)); // Read the binary representation of the size

    if (bytesRead <= 0)
    {
      error("Failed to read size of fromNode pipe");
      close(fdIn);
      return 1;
    }

    std::string data;
    data.reserve(dataSize);

    const int64_t bufferSize = 128;
    char buffer[bufferSize];
    while (dataSize > 0)
    {
      int bytesToRead = std::min(bufferSize, dataSize);
      int bytesRead = read(fdIn, buffer, bytesToRead);
      if (bytesRead <= 0)
      {
        error("Failed to read data from named pipe");
        close(fdIn);
        return 1;
      }
      data.append(buffer, bytesRead);
      dataSize -= bytesRead;
    }

    // Close the read end of the pipe
    close(fdIn);

    wait(NULL);

    if (unlink(toNodeFifo.c_str()) == -1)
    {
      perror("Error removing the toNodeFifo");
    }

    if (unlink(fromNodeFifo.c_str()) == -1)
    {
      perror("Error removing the fromNodeFifo");
    }

    MPI_Send(data.c_str(), data.size(), MPI_CHAR, 0, requestNumber, MPI_COMM_WORLD);
    return 0;
  }
  else
  { // child process
    // Exec the child.js script
    execlp("node", "node", "child.js", toNodeFifo.c_str(), fromNodeFifo.c_str(), NULL);

    // If exec returns, it must have failed
    error("Exec failed");
    return 1;
  }
}
