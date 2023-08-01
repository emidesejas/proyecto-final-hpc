#pragma once

#include <drogon/drogon.h>
#include <mpi.h>

#include "globalStructures.hpp"
#include "misc.hpp"

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
          LOG_INFO << "Invalid request: missing lambda id " << lambdaId;
          Json::Value json;
          json["error"] = "Missing lambda id";
          auto resp = HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(k400BadRequest);
          callback(resp);
        }
        else
        {
          int localRequestNumber;

          requestCounterMutex.lock();
          requestCounter++;
          localRequestNumber = int(requestCounter);
          requestCounterMutex.unlock();

          LOG_INFO << "LambdaID: " << lambdaIdInt << " with request number: " << localRequestNumber;

          stateMutex.lock();
          auto availableHandler = getAvailableHandler(handlerStates);
          if (availableHandler != 0)
          {
            handlerStates[availableHandler - 1].lambdasRunning++;
            LOG_INFO << "Lambdas running: " << handlerStates[availableHandler - 1].lambdasRunning;
          }
          stateMutex.unlock();

          auto responseHandler = [&handlerStates, availableHandler, callback, &unhandledRequests](MPI_Status status, std::string response)
          {
            LOG_INFO << "Received from mpi " << status.MPI_TAG;

            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k200OK);
            resp->setBody(response);
            resp->setContentTypeCode(CT_APPLICATION_JSON);
            callback(resp);
            LOG_INFO << "Response sent for request: " << status.MPI_TAG;
          };

          pendingRequests[localRequestNumber] = {localRequestNumber, responseHandler, app().getIOLoop(app().getCurrentThreadIndex())};

          if (availableHandler == 0)
          {
            LOG_WARN << "Request " << localRequestNumber << " will be enqued.";
            {
              std::lock_guard<std::mutex> lock(unhandledRequestsMutex);
              unhandledRequests.push({lambdaId, localRequestNumber});
            }
            return;
          }
          else
          {
            LOG_INFO << "Chosen node: " << availableHandler;
            LOG_INFO << "MPI Send to start lambda with id: " << lambdaId;
            MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, availableHandler, localRequestNumber, MPI_COMM_WORLD);
          }
        }
      },
      {Get});

  LOG_INFO << "Server running on http://127.0.0.1:8848";
  // Set the number of threads to 0 to use as many threads as available CPU cores
  // app().setThreadNum(0);
  app().addListener("127.0.0.1", 8848);
  app().setIntSignalHandler(
      []()
      {
        LOG_INFO << "Server is going to exit.";
        MPI_Finalize();
        app().quit();
      });

  app().run();
}