## Setup build
```shell
cmake -B build -S .
```

## Build project
```shell
cmake --build build 
```

## Run the project
```shell
mpirun -n 1 ./build/main : -n 1 ./build/lambdaHandler
```
