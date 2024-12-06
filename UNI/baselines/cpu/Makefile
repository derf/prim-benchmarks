.PHONY: all
all: uni

TYPE ?= int64_t

uni: app_baseline.c
	gcc -Wall -Wextra -pedantic -march=native -O2 -o uni -fopenmp -DT=${TYPE} app_baseline.c

uni_O0: app_baseline.c
	gcc -o uni_O0 -fopenmp app_baseline.c

uni_O2: app_baseline.c
	gcc -O2 -o uni_O2 -fopenmp app_baseline.c

.PHONY: run
run: uni
	./uni -i 1258291200 -t 4

# -e 20 was not part of the upstream instructions

.PHONY: run_O0
run_O0: uni_O0
	./uni_O0 -i 1258291200 -t 4 -e 20

.PHONY: run_O2
run_O2: uni_O2
	./uni_O2 -i 1258291200 -t 4 -e 20

.PHONY: clean
clean:
	rm -f uni uni_O0 uni_O2
