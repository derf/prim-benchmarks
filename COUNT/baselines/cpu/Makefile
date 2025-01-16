benchmark ?= 1
debug ?= 0
native ?= 1
nop_sync ?= 0
numa ?= 0
numa_memcpy ?= 0

CFLAGS =
LDFLAGS =

ifeq (${debug}, 1)
	CFLAGS += -g
endif

ifeq (${native}, 1)
	CFLAGS += -march=native
endif

ifeq (${numa}, 1)
	LDFLAGS += -lnuma
endif

.PHONY: all
all: count

TYPE ?= uint64_t

count: app_baseline.c
	gcc -Wall -Wextra -pedantic -O3 ${CFLAGS} -o count -DT=${TYPE} -DNUMA=${numa} -DNOP_SYNC=${nop_sync} -DWITH_BENCHMARK=${benchmark} app_baseline.c -fopenmp ${LDFLAGS}

.PHONY: run
run: count
	./count -i 1258291200 -t 4

.PHONY: clean
clean:
	rm -f count
