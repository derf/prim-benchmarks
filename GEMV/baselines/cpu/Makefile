.PHONY: all
all: gemv

gemv: gemv_openmp.c
	gcc -O2 -o gemv -fopenmp gemv_openmp.c

gemv_O0: gemv_openmp.c
	gcc -o gemv_O0 -fopenmp gemv_openmp.c

gemv_O2: gemv_openmp.c
	gcc -O2 -o gemv_O2 -fopenmp gemv_openmp.c

.PHONY: run run_O0 run_O2

run: gemv
	./gemv

run_O0: gemv_O0
	./gemv_O0

run_O2: gemv_O2
	./gemv_O2

.PHONY: clean
clean:
	rm -f gemv gemv_O0 gemv_O2
