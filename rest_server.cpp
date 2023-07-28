#include <drogon/drogon.h>
#include <mpi.h>
#include <omp.h>

using namespace drogon;

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

int getAvailableHandler(std::vector<HandlerState> handlerStates);
bool convertToInt(const std::string& str, int *result);

void restServer(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests);
void mpiHandler(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests);

int main()
{
  // Initialize the MPI environment
  int provided;
  MPI_Init_thread(NULL, NULL, MPI_THREAD_MULTIPLE, &provided);

  LOG_WARN_IF(provided < MPI_THREAD_MULTIPLE) << "MPI does not provide the required threading support";

  //MPI_Init(NULL, NULL);

  // Get the number of processes
  int world_size;
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  // Get the rank of the process
  int world_rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

  // Get the name of the processor
  char processor_name[MPI_MAX_PROCESSOR_NAME];
  int name_len;
  MPI_Get_processor_name(processor_name, &name_len);

  // Print off a hello world message
  printf("%s: Rest Server with rank %d out of %d processors\n",
          processor_name, world_rank, world_size);

  int availableLambdas[world_size];

  MPI_Gather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, availableLambdas, 1, MPI_INT, 0, MPI_COMM_WORLD);

  std::vector<HandlerState> handlerStates(world_size - 1);

  for (int i = 1; i < world_size; i++)
  {
    handlerStates[i - 1] = { availableLambdas[i], 0 };
  }

  int requestCounter = 0;

  std::map<int, PendingRequest> pendingRequests;

  omp_lock_t my_lock;
  omp_init_lock(&my_lock);

  #pragma omp parallel sections
  {
    #pragma omp section
    {
      restServer(world_size, handlerStates, requestCounter, pendingRequests);
    }
    #pragma omp section
    {
      mpiHandler(world_size, handlerStates, requestCounter, pendingRequests);
    }
  }
}

int getAvailableHandler(std::vector<HandlerState> handlerStates)
{

  int availableHandler = -1;

  int i = 0;
  while (i < handlerStates.size() && availableHandler == -1)
  {
    if (handlerStates[i].lambdasRunning < handlerStates[i].lambdas)
    {
      availableHandler = i;
    }
    i++;
  }
  
  return availableHandler + 1;
}

bool convertToInt(const std::string& str, int *result) {
  size_t pos = 0;
    
  // Try to convert the string
  try {
    *result = std::stoi(str, &pos);
  } catch (const std::exception&) {
    // Conversion failed, return false
    return false;
  }

  // Check if the whole string was processed
  return pos == str.size();
}

void restServer(int worldSize, std::vector<HandlerState> handlerStates, int &requestCounter, std::map<int, PendingRequest> &pendingRequests) {
  // `registerHandler()` adds a handler to the desired path. The handler is
  // responsible for generating a HTTP response upon an HTTP request being
  // sent to Drogon
  app().registerHandler(
    "/lambda/{lambda-id}",
    [worldSize, &handlerStates, &requestCounter, &pendingRequests](const HttpRequestPtr &request,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::string &lambdaId)
    {
      // GET REQUEST NUMBER. USE SOME VARIABLE TO ACCOUNT THE REQUEST NUMBER AND USE IT AS TAG
      auto resp = HttpResponse::newHttpResponse();
      int tag;
      if (!convertToInt(lambdaId, &tag) && lambdaId.empty())
      {
        LOG_INFO << "Invalid request: missing lambda id " << lambdaId;
        Json::Value json;
        json["error"]="Missing lambda id";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
      } else {
        LOG_INFO << "TAG: " << tag;
        

        requestCounterMutex.lock();
        requestCounter++;
        requestCounterMutex.unlock();

        stateMutex.lock();
        auto availableHandler = getAvailableHandler(handlerStates);
        handlerStates[availableHandler].lambdasRunning++;
        stateMutex.unlock();

        if (availableHandler == 0) {
          LOG_WARN << "No available handler";
          return;
        }

        auto responseHandler = [&handlerStates, availableHandler, callback](MPI_Status status, std::istringstream &response){
          LOG_INFO << "Received from mpi " << status.MPI_TAG;

          Json::Value json;
          json["status"] = "OK";

          Json::Value jsonData;
          Json::CharReaderBuilder jsonBuilder;
          std::string errs;

          LOG_INFO << "Response: " << response.str();

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

        pendingRequests[requestCounter] = { tag, responseHandler, app().getIOLoop(app().getCurrentThreadIndex()) };

        LOG_INFO << "Chosen node: " << availableHandler;
        LOG_INFO << "MPI Send to start lambda with id: " << lambdaId;
        MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, availableHandler, tag, MPI_COMM_WORLD);
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
