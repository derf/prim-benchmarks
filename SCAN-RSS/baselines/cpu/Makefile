TYPE ?= UINT64

all: scan

scan: app_baseline.cpp
	g++ -O2 app_baseline.cpp -fopenmp -DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP -lgomp -I/usr/local/cuda-8.0/include -lm -o scan -D${TYPE}

run: scan
	./scan -i 1258291200 -t 4

clean:
	rm -f scan

.PHONY: all run clean
