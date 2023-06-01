all: red

red: app_baseline.cpp
	g++ -O2 app_baseline.cpp -fopenmp -DTHRUST_HOST_SYSTEM=THRUST_HOST_SYSTEM_CPP -DTHRUST_DEVICE_SYSTEM=THRUST_DEVICE_SYSTEM_OMP -lgomp -lm -o red -D${TYPE}

run: red
	./red -i 1048576000 -t 4

clean:
	rm red

.PHONY: all clean run
