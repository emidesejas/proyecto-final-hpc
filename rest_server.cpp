#include <drogon/drogon.h>
#include <mpi.h>

using namespace drogon;

int main()
{
  // Initialize the MPI environment
  MPI_Init(NULL, NULL);

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

  // `registerHandler()` adds a handler to the desired path. The handler is
  // responsible for generating a HTTP response upon an HTTP request being
  // sent to Drogon
  app().registerHandler(
    "/lambda/{lambda-id}",
    [](const HttpRequestPtr &,
        std::function<void(const HttpResponsePtr &)> &&callback,
        const std::string &lambdaId)
    {
      auto resp = HttpResponse::newHttpResponse();
      if (lambdaId.empty())
      {
        LOG_INFO << "Invalid request: missing lambda id " << lambdaId;
        Json::Value json;
        json["error"]="Missing lambda id";
        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
      } else {
        LOG_INFO << "lambda id: " << lambdaId;
        Json::Value json;
        json["lambda"] = lambdaId;

        int number = 1;
        MPI_Send(lambdaId.c_str(), lambdaId.size(), MPI_CHAR, 1, 0, MPI_COMM_WORLD);

        auto resp = HttpResponse::newHttpJsonResponse(json);
        resp->setStatusCode(k200OK);
        callback(resp);
      }
    },
    {Get});

  // Ask Drogon to listen on 127.0.0.1 port 8848. Drogon supports listening
  // on multiple IP addresses by adding multiple listeners. For example, if
  // you want the server also listen on 127.0.0.1 port 5555. Just add another
  // line of addListener("127.0.0.1", 5555)
  LOG_INFO << "Server running on http://127.0.0.1:8848";
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
