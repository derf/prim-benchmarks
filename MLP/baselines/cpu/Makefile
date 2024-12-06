all: mlp_openmp

mlp_openmp: mlp_openmp.c
	gcc -Wall -Wextra -pedantic -march=native -O2 mlp_openmp.c -o mlp_openmp -fopenmp -std=c99

mlp_openmp_O0: mlp_openmp.c
	gcc mlp_openmp.c -o mlp_openmp_O0 -fopenmp -std=c99

mlp_openmp_O2: mlp_openmp.c
	gcc -O2 mlp_openmp.c -o mlp_openmp_O2 -fopenmp -std=c99

run: mlp_openmp
	./mlp_openmp

run_O0: mlp_openmp_O0
	./mlp_openmp_O0

run_O2: mlp_openmp_O2
	./mlp_openmp_O2

clean:
	rm -f mlp_openmp mlp_openmp_O0 mlp_openmp_O2

.PHONY: all run run_O0 run_O2 clean
