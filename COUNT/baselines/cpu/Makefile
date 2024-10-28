NUMA ?= 0
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

.PHONY: all
all: count

TYPE ?= uint64_t

count: app_baseline.c
	gcc -Wall -Wextra -pedantic -march=native -O2 -o count -fopenmp -DT=${TYPE} -DNUMA=${NUMA} app_baseline.c ${FLAGS}

.PHONY: run
run: count
	./count -i 1258291200 -t 4

.PHONY: clean
clean:
	rm -f count count_O0 count_O2
