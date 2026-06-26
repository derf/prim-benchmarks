
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <omp.h>

#if DFATOOL_TIMING
#include "../../include/timer.h"
Timer timer;
#else
#define startTimer(...)
#define stopTimer(...)
#endif

#if WITH_PERF_LIB
#include "../../../include/perf-lib.h"
#else
#define perf_start(...)
#define perf_stop(...)
#endif

#if NUMA
#include "../../../include/numa.h"
#else
#define numa_bind_alloc(size, bitmask) malloc(size)
#define numa_free(data, size) free(data)
#endif

#include "../../include/matrix.h"
#include "../../include/params.h"
#include "../../include/utils.h"

int main(int argc, char** argv)
{

	// Process parameters
	struct Params p = input_params(argc, argv);
	omp_set_num_threads(p.n_threads);

#if NUMA
	// readCOOMatrix and coo2csr call malloc → must set bitmask here already
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
	}
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	// Initialize SpMV data structures
	PRINT_INFO(p.verbosity >= 1, "Reading matrix %s", p.fileName);
	struct COOMatrix cooMatrix = readCOOMatrix(p.fileName);
	PRINT_INFO(p.verbosity >= 1, "    %u rows, %u columns, %u nonzeros",
	    cooMatrix.numRows, cooMatrix.numCols, cooMatrix.numNonzeros);
	struct CSRMatrix csrMatrix = coo2csr(cooMatrix);
	float* inVector = numa_bind_alloc(csrMatrix.numCols * sizeof(float), p.bitmask_in);
	initVector(inVector, csrMatrix.numCols);

	float* outVector = numa_bind_alloc(csrMatrix.numRows * sizeof(float), p.bitmask_out);

	for (int i = 0; i < p.n_warmup + p.n_reps; i++) {
		perf_start();
		startTimer(&timer);
#pragma omp parallel for
		for (uint32_t rowIdx = 0; rowIdx < csrMatrix.numRows; ++rowIdx) {
			float sum = 0.0f;
			for (uint32_t i = csrMatrix.rowPtrs[rowIdx];
			    i < csrMatrix.rowPtrs[rowIdx + 1]; ++i) {
				uint32_t colIdx = csrMatrix.nonzeros[i].col;
				float value = csrMatrix.nonzeros[i].value;
				sum += inVector[colIdx] * value;
			}
			outVector[rowIdx] = sum;
		}
		stopTimer(&timer);
		perf_stop();

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

#if NUMA
		numa_node_in = numa_get_node_of_page(inVector, "inVector");
		numa_node_out = numa_get_node_of_page(outVector, "outVector");
#endif

		if (i >= p.n_warmup) {
#if WITH_PERF_LIB
			printf("[::] SpMV CPU | n_threads=%u e_type=float n_elements=%u",
			    nr_threads, csrMatrix.numNonzeros);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			// coomatrix / csrmatrix use uint32_t indexes and float values
			printf("[::] SpMV CPU | n_threads=%u e_type=float n_elements=%u",
			    nr_threads, csrMatrix.numNonzeros);
#if NUMA
			printf(" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d",
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out));
#endif
			printf(" throughput_MBps=%f throughput_MOpps=%f latency_us=%f\n",
			    csrMatrix.numNonzeros * sizeof(float) / (getElapsedTime(timer) * 1e6),
			    csrMatrix.numNonzeros / (getElapsedTime(timer) * 1e6),
			    getElapsedTime(timer) * 1e6);
#endif
		}
	}

	// Deallocate data structures
	numa_free(inVector, csrMatrix.numCols * sizeof(float));
	numa_free(outVector, csrMatrix.numRows * sizeof(float));
	freeCOOMatrix(cooMatrix);
	freeCSRMatrix(csrMatrix);

	return 0;
}
