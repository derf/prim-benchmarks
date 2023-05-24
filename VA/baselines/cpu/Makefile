.PHONY: all
all: va

TYPE ?= int32_t

va: app_baseline.c
	gcc -O2 -o va -fopenmp -DT=${TYPE} app_baseline.c

va_O0: app_baseline.c
	gcc -o va_O0 -fopenmp app_baseline.c

va_O2: app_baseline.c
	gcc -O2 -o va_O2 -fopenmp app_baseline.c

.PHONY: run
run: va
	./va -t 4

.PHONY: run_O0
run_O0: va_O0
	./va_O0 -t 4

.PHONY: run_O2
run_O2: va_O2
	./va_O2 -t 4

.PHONY: clean
clean:
	rm -f va va_O0 va_O2
