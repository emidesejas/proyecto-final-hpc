#include <cstdlib>
#include <ctime>

#include <mpi.h>
#include <fmt/format.h>

#include <iostream>
#include <unistd.h>
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

    // Pipes for IPC
    int fd1[2], fd2[2];

    // Create the pipes
    if (pipe(fd1) == -1 || pipe(fd2) == -1) {
        std::cerr << "Pipe failed" << std::endl;
        return 1;
    }

    // Fork a child process
    pid_t pid = fork();

    if (pid > 0) { // parent process
        // Close the unused ends of the pipes
        close(fd1[READ_END]);
        close(fd2[WRITE_END]);

        // Write to the pipe
        std::string data = "Hello, Node.js!\n";
        write(fd1[WRITE_END], data.c_str(), data.size());

        // Close the write end of the first pipe
        close(fd1[WRITE_END]);

        // Read from the second pipe
        char buffer[128];
        read(fd2[READ_END], buffer, sizeof(buffer));

        // Print the result
        std::cout << "Received from Node.js: " << buffer << std::endl;

        // Close the read end of the second pipe
        close(fd2[READ_END]);

        // Wait for the child to exit
        wait(NULL);
    } else { // child process
        // Close the unused ends of the pipes
        close(fd1[WRITE_END]);
        close(fd2[READ_END]);

        // Redirect stdin to the read end of the first pipe
        dup2(fd1[READ_END], STDIN_FILENO);

        // Redirect stdout to the write end of the second pipe
        dup2(fd2[WRITE_END], STDOUT_FILENO);

        // Exec the Node.js script
        execlp("node", "node", "child.js", NULL);

        // If exec returns, it must have failed
        std::cerr << "Exec failed" << std::endl;
        return 1;
    }

    // fmt::println("Executing lambdas/{}.js", request_string);
    // auto output = request_string + "_" + std::to_string(std::time(nullptr));

    // std::string command = fmt::format("node lambdas/{}.js >> invocations/{}.log", request_string, output);
    // system(command.c_str());

    // fmt::println("Result can be found in {}", output);

    fmt::println("Execution finished");
  }

  MPI_Finalize();
}
