#pragma once

#include <drogon/drogon.h>
#include <mpi.h>

#include "globalStructures.hpp"
#include "misc.hpp"
#include "logger.hpp"

using namespace drogon;

void restServer(int worldSize, std::vector<HandlerState> &handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests, std::queue<UnhandledRequest> &unhandledRequests)
{
  app().registerHandler(
      "/lambda/{lambda-id}",
      [worldSize, &handlerStates, &requestCounter, &pendingRequests, &unhandledRequests](const HttpRequestPtr &request,
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
          int localRequestNumber;

          requestCounterMutex.lock();
          requestCounter++;
          localRequestNumber = int(requestCounter);
          requestCounterMutex.unlock();

          info("Request number: {} with lambda id: {}", localRequestNumber, lambdaId);

          stateMutex.lock();
          auto availableHandler = getAvailableHandler(handlerStates);
          if (availableHandler != 0)
          {
            handlerStates[availableHandler - 1].lambdasRunning++;
          }
          stateMutex.unlock();

          auto responseHandler = [&handlerStates, availableHandler, callback, &unhandledRequests](MPI_Status status, std::string response)
          {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setBody(response);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
            info("Response sent for request: {}", status.MPI_TAG);
          };

          pendingRequests[localRequestNumber] = {localRequestNumber, responseHandler, app().getIOLoop(app().getCurrentThreadIndex())};

          if (availableHandler == 0)
          {
            info("Request {} will be enqued.", localRequestNumber);
            {
              std::lock_guard<std::mutex> lock(unhandledRequestsMutex);
              unhandledRequests.push({lambdaId, localRequestNumber});
            }
            return;
          }
          else
          {
            info("Request {} will be executed on handler {}", localRequestNumber, availableHandler);
            MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, availableHandler, localRequestNumber, MPI_COMM_WORLD);
          }
        }
      },
      {Get});

  // Set the number of threads to 0 to use as many threads as available CPU cores
  // app().setThreadNum(0);
  app().addListener("127.0.0.1", 8848);
  app().setIntSignalHandler(
      []()
      {
        info("Received SIGINT signal.");
        // TODO: send signal to stop all handlers
        MPI_Finalize();
        app().quit();
      });

  info("Server running on http://127.0.0.1:8848.");
  app().run();
}