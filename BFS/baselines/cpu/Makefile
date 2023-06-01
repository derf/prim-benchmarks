.PHONY: all
all: bfs

bfs: app.c
	gcc -O2 -o bfs -fopenmp app.c

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
