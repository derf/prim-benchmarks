
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <omp.h>

#if NUMA
#include "../../../include/numa.h"
#else
#define numa_bind_alloc(size, bitmask) malloc(size)
#define numa_free(data, size) free(data)
#endif

#include "../../include/common.h"
#include "../../include/graph.h"
#include "../../include/params.h"
#include "../../include/utils.h"

#if DFATOOL_TIMING
#include "../../include/timer.h"
Timer timer;
#else
#define startTimer(...)
#define stopTimer(...)
#endif

#if WITH_PERF_LIB
#include "../../../include/perf-lib.h"
#elif WITH_PERF_EXT
#include "../../../include/perf-ext.h"
#else
#define perf_start(...)
#define perf_stop(...)
#endif

int main(int argc, char** argv)
{

	// Process parameters
	struct Params p = input_params(argc, argv);

	omp_set_num_threads(p.n_threads);

#if NUMA
	if (p.bitmask_in) {
		numa_set_membind(p.bitmask_in);
	}
	numa_node_cpu = numa_cpu_bind(p.numa_node_cpu);
#endif

	// Initialize BFS data structures
	PRINT_INFO(p.verbosity >= 1, "Reading graph %s", p.fileName);
	struct COOGraph cooGraph = readCOOGraph(p.fileName);
	PRINT_INFO(p.verbosity >= 1, "    Graph has %d nodes and %d edges", cooGraph.numNodes, cooGraph.numEdges);

	for (int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

		struct CSRGraph csrGraph = coo2csr(cooGraph);
		uint32_t* nodeLevel = (uint32_t*)numa_bind_alloc(csrGraph.numNodes * sizeof(uint32_t), p.bitmask_in);
		uint32_t* nodeLevelRef = (uint32_t*)numa_bind_alloc(csrGraph.numNodes * sizeof(uint32_t), p.bitmask_in);
		for (uint32_t i = 0; i < csrGraph.numNodes; ++i) {
			nodeLevel[i] = UINT32_MAX; // Unreachable
			nodeLevelRef[i] = UINT32_MAX; // Unreachable
		}
		uint32_t srcNode = 0;

		// Initialize frontier double buffers
		uint32_t* buffer1 = (uint32_t*)numa_bind_alloc(csrGraph.numNodes * sizeof(uint32_t), p.bitmask_in);
		uint32_t* buffer2 = (uint32_t*)numa_bind_alloc(csrGraph.numNodes * sizeof(uint32_t), p.bitmask_in);
		uint32_t* prevFrontier = buffer1;
		uint32_t* currFrontier = buffer2;

#if NOP_SYNC
		for (int rep = 0; rep < 200000; rep++) {
			asm volatile("nop" ::);
		}
#endif

		// Calculating result on CPU
		perf_start();
		startTimer(&timer, 0, 0);
		nodeLevel[srcNode] = 0;
		prevFrontier[0] = srcNode;
		uint32_t numPrevFrontier = 1;
		for (uint32_t level = 1; numPrevFrontier > 0; ++level) {

			uint32_t numCurrFrontier = 0;

// Visit nodes in the previous frontier
#pragma omp parallel for
			for (uint32_t i = 0; i < numPrevFrontier; ++i) {
				uint32_t node = prevFrontier[i];
				for (uint32_t edge = csrGraph.nodePtrs[node]; edge < csrGraph.nodePtrs[node + 1]; ++edge) {
					uint32_t neighbor = csrGraph.neighborIdxs[edge];
					uint32_t justVisited = 0;
#pragma omp critical
					{
						if (nodeLevel[neighbor] == UINT32_MAX) { // Node not previously visited
							nodeLevel[neighbor] = level;
							justVisited = 1;
						}
					}
					if (justVisited) {
						uint32_t currFrontierIdx;
#pragma omp critical
						{
							currFrontierIdx = numCurrFrontier++;
						}
						currFrontier[currFrontierIdx] = neighbor;
					}
				}
			}

			// Swap buffers
			uint32_t* tmp = prevFrontier;
			prevFrontier = currFrontier;
			currFrontier = tmp;
			numPrevFrontier = numCurrFrontier;
		}
		stopTimer(&timer, 0);
		perf_stop();

#if NOP_SYNC
		for (int rep = 0; rep < 200000; rep++) {
			asm volatile("nop" ::);
		}
#endif

		freeCSRGraph(csrGraph);
		numa_free(buffer1, csrGraph.numNodes * sizeof(uint32_t));
		numa_free(buffer2, csrGraph.numNodes * sizeof(uint32_t));

		csrGraph = coo2csr(cooGraph);
		srcNode = 0;

		// Initialize frontier double buffers
		buffer1 = (uint32_t*)malloc(csrGraph.numNodes * sizeof(uint32_t));
		buffer2 = (uint32_t*)malloc(csrGraph.numNodes * sizeof(uint32_t));
		prevFrontier = buffer1;
		currFrontier = buffer2;

		// Calculating result on CPU sequentially
		if (rep >= p.n_warmup) {
			startTimer(&timer, 1, 0);
		}
		nodeLevelRef[srcNode] = 0;
		prevFrontier[0] = srcNode;
		numPrevFrontier = 1;
		for (uint32_t level = 1; numPrevFrontier > 0; ++level) {

			uint32_t numCurrFrontier = 0;

			// Visit nodes in the previous frontier
			for (uint32_t i = 0; i < numPrevFrontier; ++i) {
				uint32_t node = prevFrontier[i];
				for (uint32_t edge = csrGraph.nodePtrs[node]; edge < csrGraph.nodePtrs[node + 1]; ++edge) {
					uint32_t neighbor = csrGraph.neighborIdxs[edge];
					uint32_t justVisited = 0;
					if (nodeLevelRef[neighbor] == UINT32_MAX) { // Node not previously visited
						nodeLevelRef[neighbor] = level;
						justVisited = 1;
					}
					if (justVisited) {
						uint32_t currFrontierIdx;
						currFrontierIdx = numCurrFrontier++;
						currFrontier[currFrontierIdx] = neighbor;
					}
				}
			}

			// Swap buffers
			uint32_t* tmp = prevFrontier;
			prevFrontier = currFrontier;
			currFrontier = tmp;
			numPrevFrontier = numCurrFrontier;
		}
		if (rep >= p.n_warmup) {
			stopTimer(&timer, 1);
		}

#if NUMA
		numa_node_in = numa_get_node_of_page(csrGraph.nodePtrs, "csrGraph.nodePtrs");
#endif

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		// Verifying result
		int isOK = 1;
		for (uint32_t nodeIdx = 0; nodeIdx < csrGraph.numNodes; ++nodeIdx) {
			if (nodeLevel[nodeIdx] != nodeLevelRef[nodeIdx]) {
				PRINT_ERROR("Mismatch at node %u (CPU sequential result = level %u, CPU parallel result = level %u)", nodeIdx, nodeLevelRef[nodeIdx], nodeLevel[nodeIdx]);
				isOK = 0;
			}
		}

		if (isOK && (rep >= p.n_warmup)) {
#if WITH_PERF_LIB
			printf("[::] BFS CPU | n_threads=%d e_type=%s n_elements=%d", nr_threads, "uint32_t", csrGraph.numNodes);
#if NUMA
			printf(" numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d", numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu));
#endif
			printf(" |");
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] BFS CPU | n_threads=%d e_type=%s n_elements=%d",
			    nr_threads, "uint32_t", csrGraph.numNodes);
#if NUMA
			printf(" numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d",
			    numa_node_in, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu));
#endif
			printf(" | throughput_seq_MBps=%f throughput_MBps=%f",
			    csrGraph.numNodes * sizeof(uint32_t) / timer.time[1],
			    csrGraph.numNodes * sizeof(uint32_t) / timer.time[0]);
			printf(" throughput_seq_MOpps=%f throughput_MOpps=%f",
			    csrGraph.numNodes / timer.time[1],
			    csrGraph.numNodes / timer.time[0]);
			printf(" latency_us=%f latency_seq_us=%f\n",
			    timer.time[0],
			    timer.time[1]);
#endif // DFATOOL_TIMING
		}

		freeCSRGraph(csrGraph);
		numa_free(nodeLevel, csrGraph.numNodes * sizeof(uint32_t));
		numa_free(nodeLevelRef, csrGraph.numNodes * sizeof(uint32_t));
		numa_free(buffer1, csrGraph.numNodes * sizeof(uint32_t));
		numa_free(buffer2, csrGraph.numNodes * sizeof(uint32_t));
	}

	// Deallocate data structures
	freeCOOGraph(cooGraph);

	return 0;
}
