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
mpirun -n 1 ./build/rest_server : -n 1 ./build/lambda_handler
```
