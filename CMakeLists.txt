cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(trabajo-final-hpc CXX)

include(ExternalProject)
include(ProcessorCount)

# Get the number of processors
ProcessorCount(processors)

ExternalProject_Add(
  nodejs
  GIT_REPOSITORY "https://github.com/nodejs/node.git"
  GIT_TAG "v18.16.1"
  PREFIX "${CMAKE_CURRENT_BINARY_DIR}/nodejs"
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> ./configure --prefix=<INSTALL_DIR> --shared
  BUILD_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> make -j${processors}
  INSTALL_COMMAND ${CMAKE_COMMAND} -E chdir <SOURCE_DIR> make install
)

# Set the path to Node.js binary and include directories
set(NODEJS_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/nodejs/src/nodejs/out")
set(NODEJS_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/nodejs/include/node")

# Get the path to the Node.js library file
set(NODEJS_LIB_FILE "${CMAKE_CURRENT_BINARY_DIR}/nodejs/lib/libnode.108.dylib")

# Set the path to the directory containing the Node.js library
set(NODEJS_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/nodejs/lib")

# Find the Node.js library
message("NODEJS_LIB_DIR is ${NODEJS_LIB_DIR}")

find_library(NODEJS_LIB
  NAMES node libnode node.108 libnode.108
  PATHS ${NODEJS_LIB_DIR}
  NO_DEFAULT_PATH)

message("NODEJS_LIB is ${NODEJS_LIB}")

# Add Node.js include directories
include_directories(${NODEJS_INCLUDE_DIR})

# source: https://cliutils.gitlab.io/modern-cmake/chapters/packages/MPI.html
find_package(MPI REQUIRED)

find_package(Drogon REQUIRED)
find_package(fmt REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(rest_server rest_server.cpp)
add_executable(lambda_handler lambda_handler.cpp)

target_link_libraries(rest_server
  PRIVATE
    Drogon::Drogon
    MPI::MPI_CXX)

target_link_libraries(lambda_handler
  PRIVATE
    MPI::MPI_CXX
    fmt::fmt
    ${NODEJS_LIB})
