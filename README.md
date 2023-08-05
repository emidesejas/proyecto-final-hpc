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
mpirun -n 1 ./build/loadBalancer : -n 1 ./build/lambdaHandler -l 2
```
