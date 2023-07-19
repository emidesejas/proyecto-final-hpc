#include <cstdlib>
#include <ctime>

#include <mpi.h>
#include <fmt/format.h>

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define READ_END 0
#define WRITE_END 1

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

    // Fork a child process
    pid_t pid = fork();

    auto fifoSuffix = fmt::format("{}_{}", world_rank, request_string);
    auto toNodeFifo = fmt::format("toNode_{}", fifoSuffix);
    auto fromNodeFifo = fmt::format("fromNode_{}", fifoSuffix);

    if (pid > 0) { // parent process
      // Create the named pipes
      mkfifo(toNodeFifo.c_str(), 0666);
      mkfifo(fromNodeFifo.c_str(), 0666);

      // Open the toNodeFifo for writing
      int fd_out = open(toNodeFifo.c_str(), O_WRONLY);
      if (fd_out == -1) {
        std::cerr << "Failed to open toNodeFifo for writing" << std::endl;
        return 1;
      }

      // Write to node
      //std::string data = "Hello, Node.js!";
      write(fd_out, request_string.c_str(), request_string.size());

      // Close the write end of the pipe
      close(fd_out);

      // Open the fromNodeFifo for reading
      int fd_in = open(fromNodeFifo.c_str(), O_RDONLY);
      if (fd_in == -1) {
        std::cerr << "Failed to open fromNodeFifo for reading" << std::endl;
        return 1;
      }

      // Read from the IN_FIFO
      char buffer[128];
      int numBytes = read(fd_in, buffer, sizeof(buffer));
      buffer[numBytes] = '\0'; // Null-terminate the string

      // Print the result
      std::cout << "Received from Node.js: " << buffer << std::endl;
      
      // Close the read end of the pipe
      close(fd_in);

      wait(NULL);

      if (unlink(toNodeFifo.c_str()) == -1) {
        perror("Error removing the toNodeFifo");
      }

      if (unlink(fromNodeFifo.c_str()) == -1) {
        perror("Error removing the fromNodeFifo");
      }
    } else { // child process
      // Exec the Node.js script
      execlp("node", "node", "child.js", toNodeFifo.c_str(), fromNodeFifo.c_str(), NULL);

      // If exec returns, it must have failed
      std::cerr << "Exec failed" << std::endl;
      return 1;
    }

    fmt::println("Execution finished");
  }

  MPI_Finalize();
}
