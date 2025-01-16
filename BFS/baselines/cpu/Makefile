benchmark ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0

LDFLAGS =
CFLAGS =

ifeq (${debug}, 1)
	CFLAGS += -g
endif

ifeq (${native}, 1)
	CFLAGS += -march=native
endif

ifeq (${numa}, 1)
	LDFLAGS += -lnuma
endif

bfs: app.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -DNUMA=${numa} -DNUMA_MEMCPY=${numa_memcpy} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${benchmark} -o bfs -fopenmp app.c ${LDFLAGS}

bfs_O0: app.c
	gcc -o bfs_O0 -fopenmp app.c

bfs_O2: app.c
	gcc -O2 -o bfs_O2 -fopenmp app.c

# each bfs invocation performs 100 iterations

.PHONY: run
run: bfs
	./bfs -f ../../data/loc-gowalla_edges.txt

.PHONY: run_O0
run_O0: bfs_O0
	OMP_NUM_THREADS=4 ./bfs_O0 -f ../../data/loc-gowalla_edges.txt

.PHONY: run_O2
run_O2: bfs_O2
	OMP_NUM_THREADS=4 ./bfs_O2 -f ../../data/loc-gowalla_edges.txt

.PHONY: clean
clean:
	rm -f bfs bfs_O0 bfs_O2

.PHONY: all
