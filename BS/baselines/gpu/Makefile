aspectc ?= 0
aspectc_timing ?= 0
verbose ?= 0

NVCC ?= /usr/local/cuda/bin/nvcc
CXX = g++

COMMON_FLAGS := -I/usr/local/cuda/include -O2 -lm
COMMON_FLAGS += -DASPECTC=${aspectc}

ifeq (${aspectc_timing}, 1)
	ASPECTC_HOST_FLAGS += -ainclude/dfatool_host_gpu.ah -ainclude/dfatool.ah
endif

ASPECTC_HOST_FLAGS ?= -a0

ifeq (${aspectc}, 1)
	CXX = ag++ -r repo.acp -v 0 ${ASPECTC_HOST_FLAGS} --c_compiler g++ -p . --Xcompiler
endif

QUIET = @

ifeq (${verbose}, 1)
	QUIET =
endif

all: cu_binary_search.so

kernel.o: kernel.cu
	${QUIET}${NVCC} ${COMMON_FLAGS} -Xcompiler -fPIC $^ -c -o $@

bs.o: bs.cc
	${QUIET}mkdir -p include
	${QUIET}cp ../../../include/dfatool_host_gpu.ah include
	${QUIET}${CXX} ${COMMON_FLAGS} -fPIC $^ -c -o $@
	${QUIET}rm -f include/dfatool_host_gpu.ah

cu_binary_search.so: bs.o kernel.o
	${QUIET}${NVCC} -m64 -Xcompiler -fPIC -shared -o $@ $^ -std=c++11 -O2

clean:
	${QUIET}rm -f cu_binary_search.so bs.o kernel.o

.PHONY: all clean
