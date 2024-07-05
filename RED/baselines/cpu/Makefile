NUMA ?= 0
TYPE ?= UINT64
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

all: red

red: app_baseline.cpp
	g++ -Wall -Wextra -pedantic -march=native -O2 app_baseline.cpp -fopenmp -DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP -DNUMA=${NUMA} -lgomp -lm -o red -D${TYPE} ${FLAGS}

run: red
	./red -i 1048576000 -t 4

clean:
	rm red

.PHONY: all clean run
