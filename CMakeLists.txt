cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(trabajo-final-hpc CXX)

# source: https://cliutils.gitlab.io/modern-cmake/chapters/packages/MPI.html
find_package(MPI REQUIRED)

find_package(Drogon REQUIRED)

set(CMAKE_CXX_STANDARD 17)

add_executable(rest_server rest_server.cpp)
add_executable(lambda_handler lambda_handler.cpp)

target_link_libraries(rest_server
  PRIVATE
    Drogon::Drogon
    MPI::MPI_CXX)

target_link_libraries(lambda_handler
  PRIVATE
    MPI::MPI_CXX)