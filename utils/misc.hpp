#pragma once

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
