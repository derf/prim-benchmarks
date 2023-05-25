.PHONY: all
all: sel

TYPE ?= uint64_t

sel: app_baseline.c
	gcc -O2 -o sel -fopenmp -DT=${TYPE} app_baseline.c

sel_O0: app_baseline.c
	gcc -o sel_O0 -fopenmp app_baseline.c

sel_O2: app_baseline.c
	gcc -O2 -o sel_O2 -fopenmp app_baseline.c

.PHONY: run
run: sel
	./sel -i 1258291200 -t 4

.PHONY: run_O0
run_O0: sel_O0
	./sel_O0 -i 1258291200 -t 4

.PHONY: run_O2
run_O2: sel_O2
	./sel_O2 -i 1258291200 -t 4

.PHONY: clean
clean:
	rm -f sel sel_O0 sel_O2
