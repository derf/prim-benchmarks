dfatool_timing ?= 1
numa ?= 0

TYPE ?= uint64_t

FLAGS += -DDFATOOL_TIMING=${dfatool_timing}

ifeq (${numa}, 1)
	FLAGS += -lnuma
endif

sel: sel.c
	gcc -Wall -Wextra -pedantic -march=native -O2 -o sel -fopenmp -DT=${TYPE} -DNUMA=${numa} sel.c ${FLAGS}

.PHONY: clean
	rm -f sel
