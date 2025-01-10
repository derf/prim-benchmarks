native ?= 1

CFLAGS =

ifeq (${native}, 1)
	CFLAGS += -march=native
endif

all: spmv

spmv: app.c
	gcc -Wall -Wextra -pedantic ${CFLAGS} -O3 -o spmv -fopenmp app.c

spmv_O0: app.c
	gcc -o spmv_O0 -fopenmp app.c

spmv_O2: app.c
	gcc -O2 -o spmv_O2 -fopenmp app.c

run: spmv
	OMP_NUM_THREADS=4 ./spmv -f ../../data/bcsstk30.mtx -v 0

run_O0: spmv_O0
	OMP_NUM_THREADS=4 ./spmv_O0 -f ../../data/bcsstk30.mtx -v 0

run_O2: spmv_O2
	OMP_NUM_THREADS=4 ./spmv_O2 -f ../../data/bcsstk30.mtx -v 0

clean:
	rm -f spmv spmv_O0 spmv_O2

.PHONY: all run run_O0 run_O2 clean
