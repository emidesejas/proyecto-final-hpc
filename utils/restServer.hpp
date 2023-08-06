#pragma once

#include <chrono>
#include <iostream>
#include <fstream>

#include <drogon/drogon.h>
#include <mpi.h>

#include "globalStructures.hpp"
#include "misc.hpp"
#include "logger.hpp"

using namespace drogon;

void restServer(
    int worldSize,
    std::vector<HandlerState> &handlerStates,
    int &requestCounter,
    std::map<int, PendingRequest> &pendingRequests,
    std::queue<UnhandledRequest> &unhandledRequests,
    std::map<int, RequestDuration> &durations,
    std::vector<RequestTimeEvent> &timeEvents)
{
  app().registerHandler(
      "/lambda/{lambda-id}",
      [&handlerStates,
       &requestCounter,
       &pendingRequests,
       &unhandledRequests,
       &durations,
       &timeEvents](const HttpRequestPtr &request,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    const std::string &lambdaId)
      {
        int lambdaIdInt;
        if (!convertToInt(lambdaId, &lambdaIdInt) && lambdaId.empty())
        {
          warn("Invalid or missing lambda id: {}", lambdaId);
          Json::Value json;
          json["error"] = "Invalid or missing lambda id";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          return;
        }
        else
        {
          auto start = std::chrono::high_resolution_clock::now();
          int localRequestNumber;

          requestCounterMutex.lock();
          requestCounter++;
          localRequestNumber = requestCounter;
          requestCounterMutex.unlock();

          durations[localRequestNumber] = {
              lambdaId,
              localRequestNumber,
              start,
              std::chrono::high_resolution_clock::from_time_t(0),
              0};

          info("Request number: {} with lambda id: {}", localRequestNumber, lambdaId);

          {
            std::lock_guard<std::mutex> lock(timeEventsMutex);
            timeEvents.push_back({localRequestNumber, start, "START"});
          }

          stateMutex.lock();
          auto availableHandler = getAvailableHandler(handlerStates);
          if (availableHandler != 0)
          {
            handlerStates[availableHandler - 1].lambdasRunning++;
          }
          stateMutex.unlock();

          auto responseHandler = [localRequestNumber, callback, &durations, &timeEvents](MPI_Request *mpiRequest, char *responseBuffer, int responseLength)
          {
            MPI_Status status;
            MPI_Wait(mpiRequest, &status);
            auto mpiWait = std::chrono::high_resolution_clock::now();
            {
              std::lock_guard<std::mutex> lock(timeEventsMutex);
              timeEvents.push_back({localRequestNumber, mpiWait, "SERVER_MPI_WAIT"});
            }
            std::string response(responseBuffer, responseLength);

            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setBody(response);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            resp->addHeader("RequestNumber", std::to_string(localRequestNumber));
            callback(resp);
            info("Response sent for request: {}", status.MPI_TAG);
            auto start = durations[localRequestNumber].startTime;
            auto end = std::chrono::high_resolution_clock::now();
            durations[localRequestNumber].endTime = end;
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            durations[localRequestNumber].duration = duration.count();

            {
              std::lock_guard<std::mutex> lock(timeEventsMutex);
              timeEvents.push_back({localRequestNumber, end, "END"});
            }

            info("Request number: {} took {} ms", localRequestNumber, duration.count());
            delete[] responseBuffer;
            delete mpiRequest;
          };

          {
            std::lock_guard<std::mutex> lock(pendingRequestsMutex);
            pendingRequests[localRequestNumber] = {localRequestNumber, responseHandler, app().getIOLoop(app().getCurrentThreadIndex())};
            info("Inserted pending requests for request number: {}", localRequestNumber);
          }

          info("Request number: {} will be executed on thread {}", localRequestNumber, app().getCurrentThreadIndex());

          if (availableHandler == 0)
          {
            info("Request {} will be enqueued.", localRequestNumber);
            auto enqueued = std::chrono::high_resolution_clock::now();
            {
              std::lock_guard<std::mutex> lock(timeEventsMutex);
              timeEvents.push_back({localRequestNumber, enqueued, "ENQUEUED"});
            }
            {
              std::lock_guard<std::mutex> lock(unhandledRequestsMutex);
              unhandledRequests.push({lambdaId, localRequestNumber});
            }
            return;
          }
          else
          {
            info("Request number: {} will be executed on handler {}", localRequestNumber, availableHandler);
            MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, availableHandler, localRequestNumber, MPI_COMM_WORLD);
            auto mpiSend = std::chrono::high_resolution_clock::now();
            {
              std::lock_guard<std::mutex> lock(timeEventsMutex);
              timeEvents.push_back({localRequestNumber, mpiSend, "SERVER_MPI_SEND"});
            }
            info("Request number: {} sent to handler {}", localRequestNumber, availableHandler);
          }
        }
      },
      {Get});

  // Set the number of threads to 0 to use as many threads as available CPU cores
  app().setThreadNum(6);
  app().addListener("127.0.0.1", 8848);
  app().setIdleConnectionTimeout(0);
  app().setIntSignalHandler(
      [&durations, &timeEvents, worldSize]()
      {
        info("Received SIGINT signal.");
        std::ofstream durationResults;
        auto currentTimestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto fileName = fmt::format("durations/{}_durationResults.csv", currentTimestamp);
        info("Saving durations for {} requests in {}", durations.size(), fileName);
        durationResults.open(fileName);
        durationResults << "Lambda ID,RequestNumber,Duration\n";

        for (auto it = durations.cbegin(); it != durations.cend(); ++it)
        {
          durationResults << fmt::format("{},{},{}\n", it->second.lambdaId, it->second.requestNumber, it->second.duration);
        }
        durationResults.close();

        fileName = fmt::format("durations/{}_timeEvents.csv", currentTimestamp);

        std::ofstream timeEventsResults;

        info("Saving {} time events for {} requests in {}", timeEvents.size(), durations.size(), fileName);
        timeEventsResults.open(fileName);
        timeEventsResults << "RequestNumber,Timestamp,Event\n";
        for (auto it = timeEvents.cbegin(); it != timeEvents.cend(); ++it)
        {
          auto miliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(it->time.time_since_epoch()).count();
          timeEventsResults << fmt::format("{},{},{}\n", it->requestNumber, miliseconds, it->event);
        }

        timeEventsResults.close();

        // TODO: send signal to stop all handlers

        for (int i = 0; i < worldSize; i++)
        {
          std::string message = "EXIT";
          MPI_Send(message.c_str(), message.length(), MPI_CHAR, i, 0, MPI_COMM_WORLD);
        }
        MPI_Finalize();
        app().quit();
      });

  info("Server running on http://127.0.0.1:8848.");
  app().run();
}