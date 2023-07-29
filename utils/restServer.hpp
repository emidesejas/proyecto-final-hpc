#pragma once

#include <drogon/drogon.h>
#include <mpi.h>

#include "globalStructures.hpp"
#include "misc.hpp"

using namespace drogon;

void restServer(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests) {
  app().registerHandler(
    "/lambda/{lambda-id}",
    [worldSize, &handlerStates, &requestCounter, &pendingRequests](const HttpRequestPtr &request,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::string &lambdaId)
    {
      // GET REQUEST NUMBER. USE SOME VARIABLE TO ACCOUNT THE REQUEST NUMBER AND USE IT AS TAG
      auto resp = HttpResponse::newHttpResponse();
      int lambdaIdInt;
      if (!convertToInt(lambdaId, &lambdaIdInt) && lambdaId.empty())
      {
        LOG_INFO << "Invalid request: missing lambda id " << lambdaId;
        Json::Value json;
        json["error"]="Missing lambda id";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
      } else {
        int localRequestNumber;

        requestCounterMutex.lock();
        requestCounter++;
        localRequestNumber = int(requestCounter);
        requestCounterMutex.unlock();

        LOG_INFO << "LambdaID: " << lambdaIdInt << " with request number: " << localRequestNumber;

        stateMutex.lock();
        auto availableHandler = getAvailableHandler(handlerStates);
        handlerStates[availableHandler].lambdasRunning++;
        stateMutex.unlock();

        if (availableHandler == 0) {
          LOG_WARN << "No available handler";

          // Should put request in queue
          return;
        }

        auto responseHandler = [&handlerStates, availableHandler, callback](MPI_Status status, std::istringstream &response){
          LOG_INFO << "Received from mpi " << status.MPI_TAG;

          Json::Value json;
          json["status"] = "OK";

          Json::Value jsonData;
          Json::CharReaderBuilder jsonBuilder;
          std::string errs;

          if (!Json::parseFromStream(jsonBuilder, response, &jsonData, &errs)) {
            std::cerr << "Failed to parse JSON: " << errs << std::endl;
            json["status"] = "ERROR";
          }

          LOG_INFO << "has parsed";

          json["response"] = jsonData;

          auto resp = HttpResponse::newHttpJsonResponse(json);
          resp->setStatusCode(k200OK);
          callback(resp);


          stateMutex.lock();
          handlerStates[availableHandler].lambdasRunning--;
          stateMutex.unlock();
        };

        pendingRequests[requestCounter] = { localRequestNumber, responseHandler, app().getIOLoop(app().getCurrentThreadIndex()) };

        LOG_INFO << "Chosen node: " << availableHandler;
        LOG_INFO << "MPI Send to start lambda with id: " << lambdaId;
        MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, availableHandler, localRequestNumber, MPI_COMM_WORLD);
      }
    },
    {Get});

  // Ask Drogon to listen on 127.0.0.1 port 8848. Drogon supports listening
  // on multiple IP addresses by adding multiple listeners. For example, if
  // you want the server also listen on 127.0.0.1 port 5555. Just add another
  // line of addListener("127.0.0.1", 5555)
  LOG_INFO << "Server running on http://127.0.0.1:8848";
  // Set the number of threads to 0 to use as many threads as available CPU cores
  //app().setThreadNum(0);
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