#pragma once

#include <unistd.h>
#include "globalStructures.hpp"

int getAvailableHandler(std::vector<HandlerState> handlerStates) {
  int availableHandler = -1;

  int i = 0;
  while (i < handlerStates.size() && availableHandler == -1) {
    if (handlerStates[i].lambdasRunning < handlerStates[i].lambdas) {
      availableHandler = i;
    }
    i++;
  }
  
  return availableHandler + 1;
}

bool convertToInt(const std::string& str, int *result) {
  size_t pos = 0;

  try {
    *result = std::stoi(str, &pos);
  } catch (const std::exception&) {
    // Conversion failed, return false
    return false;
  }

  // Check if the whole string was processed
  return pos == str.size();
}

bool MPI_Probe_timeout(int source, int tag, MPI_Comm comm, MPI_Status *status, double timeout) {
    double elapsed_time = 0.0;
    double start_time = MPI_Wtime();

    while (elapsed_time < timeout) {
      LOG_INFO << "Waiting for message";
      int flag = 0;
      MPI_Iprobe(source, tag, comm, &flag, status);
      
      if (flag) {
        return true;
      }

      // 10ms
      usleep(10000);

      double current_time = MPI_Wtime();
      elapsed_time = current_time - start_time;
    }

    return false;  // Timeout
}
