NUMA ?= 0
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

all: hist

hist: app_baseline.c
	gcc -Wall -Wextra -pedantic -O2 -DNUMA=${NUMA} -o hist -fopenmp app_baseline.c ${FLAGS}

hist_O0: app_baseline.c
	gcc -o hist_O0 -fopenmp app_baseline.c

hist_O2: app_baseline.c
	gcc -O2 -o hist_O2 -fopenmp app_baseline.c

run: hist
	./hist -i 1006632960 -t 4

run_O0: hist_O0
	./hist_O0 -i 1006632960 -t 4

run_O2: hist_O2
	./hist_O2 -i 1006632960 -t 4

clean:
	rm -f hist hist_O0 hist_O2

.PHONY: all run run_O0 run_O2 clean
