NUMA ?= 0
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

TYPE ?= UINT64

all: scan

scan: app_baseline.cpp
	g++ -Wall -Wextra -pedantic -march=native -O2 app_baseline.cpp -fopenmp -DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP -lgomp -I/usr/local/cuda-8.0/include -lm -o scan -DNUMA=${NUMA} -D${TYPE} ${FLAGS}

run: scan
	./scan -i 1258291200 -t 4

clean:
	rm -f scan

.PHONY: all run clean
