NUMA ?= 0
FLAGS =

ifeq (${NUMA}, 1)
	FLAGS += -lnuma
endif

.PHONY: all
all: va

TYPE ?= int32_t

va: app_baseline.c
	gcc -Wall -Wextra -pedantic -march=native -O2 -o va -fopenmp -DNUMA=${NUMA} -DT=${TYPE} app_baseline.c ${FLAGS}

va_O0: app_baseline.c
	gcc -o va_O0 -fopenmp app_baseline.c

va_O2: app_baseline.c
	gcc -O2 -o va_O2 -fopenmp app_baseline.c

.PHONY: run
run: va
	./va -t 4

# -w/-e are not part of upstream config

.PHONY: run_O0
run_O0: va_O0
	./va_O0 -t 4 -w 1 -e 100

.PHONY: run_O2
run_O2: va_O2
	./va_O2 -t 4 -w 1 -e 100

.PHONY: clean
clean:
	rm -f va va_O0 va_O2
