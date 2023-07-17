#include <cstdlib>
#include <ctime>

#include <mpi.h>
#include <fmt/format.h>

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
  printf("%s: Lambda handler with rank %d out of %d processors\n",
          processor_name, world_rank, world_size);

  MPI_Status status;

  while (true)
  {
    MPI_Probe(0, 0, MPI_COMM_WORLD, &status);

    int number_amount;

    MPI_Get_count(&status, MPI_CHAR, &number_amount);

    char request[number_amount];

    MPI_Recv(&request, number_amount, MPI_CHAR, 0, 0, MPI_COMM_WORLD, &status);

    // Convert the char array to std::string
    std::string request_string(request, number_amount);

    if (request_string == "0")
    {
      break;
    }

    fmt::println("Executing lambdas/{}.js", request_string);
    auto output = request_string + "_" + std::to_string(std::time(nullptr));

    std::string command = fmt::format("node lambdas/{}.js >> invocations/{}.log", request_string, output);
    system(command.c_str());

    fmt::println("Result can be found in {}", output);
  }

  MPI_Finalize();
}
