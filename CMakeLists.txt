cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
  CACHE STRING "Vcpkg toolchain file")

project(trabajo-final-hpc CXX)

# source: https://cliutils.gitlab.io/modern-cmake/chapters/packages/MPI.html
find_package(MPI REQUIRED)

find_package(OpenMP REQUIRED)
find_package(Drogon REQUIRED)
find_package(fmt REQUIRED)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(main main.cpp)
add_executable(lambdaHandler lambdaHandler.cpp)

target_link_libraries(main
  PRIVATE
    Drogon::Drogon
    MPI::MPI_CXX
    OpenMP::OpenMP_CXX
)

target_link_libraries(lambdaHandler
  PRIVATE
    MPI::MPI_CXX
    fmt::fmt
)
