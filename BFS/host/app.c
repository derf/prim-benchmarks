/**
* app.c
* BFS Host Application Source File
*
*/
#if ASPECTC
extern "C" {
#endif

#include <dpu.h>
#include <dpu_log.h>

#ifndef ENERGY
#define ENERGY 0
#endif
#if ENERGY
#include <dpu_probe.h>
#endif

#if ASPECTC
}
#endif

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mram-management.h"
#include "common.h"
#include "graph.h"
#include "params.h"
#include "timer.h"
#include "utils.h"

#define DPU_BINARY "./bin/dpu_code"

// Main of the Host Application
int main(int argc, char **argv)
{

	// Process parameters
	struct Params p = input_params(argc, argv);

	// Timer and profiling
	Timer timer;
#if ENERGY
	struct dpu_probe_t probe;
	DPU_ASSERT(dpu_probe_init("energy_probe", &probe));
	double tenergy = 0;
#endif

	// Allocate DPUs and load binary
	struct dpu_set_t dpu_set, dpu;
	uint32_t numDPUs, numRanks;

#if WITH_ALLOC_OVERHEAD
	startTimer(&timer, 0, 0);
#endif
	DPU_ASSERT(dpu_alloc(NR_DPUS, NULL, &dpu_set));
#if WITH_ALLOC_OVERHEAD
	stopTimer(&timer, 0);
#else
	zeroTimer(&timer, 0);
#endif

#if WITH_LOAD_OVERHEAD
	startTimer(&timer, 1, 0);
#endif
	DPU_ASSERT(dpu_load(dpu_set, DPU_BINARY, NULL));
#if WITH_LOAD_OVERHEAD
	stopTimer(&timer, 0);
#else
	zeroTimer(&timer, 1);
#endif

	DPU_ASSERT(dpu_get_nr_dpus(dpu_set, &numDPUs));
	DPU_ASSERT(dpu_get_nr_ranks(dpu_set, &numRanks));
	assert(NR_DPUS == numDPUs);
	PRINT_INFO(p.verbosity >= 1, "Allocated %d DPU(s)", numDPUs);

	// Initialize BFS data structures
	PRINT_INFO(p.verbosity >= 1, "Reading graph %s", p.fileName);
	struct COOGraph cooGraph = readCOOGraph(p.fileName);
	PRINT_INFO(p.verbosity >= 1, "    Graph has %d nodes and %d edges",
		   cooGraph.numNodes, cooGraph.numEdges);
	struct CSRGraph csrGraph = coo2csr(cooGraph);
	uint32_t numNodes = csrGraph.numNodes;
	uint32_t *nodePtrs = csrGraph.nodePtrs;
	uint32_t *neighborIdxs = csrGraph.neighborIdxs;
	uint32_t *nodeLevel = (uint32_t*)calloc(numNodes, sizeof(uint32_t));	// Node's BFS level (initially all 0 meaning not reachable)
	uint64_t *visited = (uint64_t*)calloc(numNodes / 64, sizeof(uint64_t));	// Bit vector with one bit per node
	uint64_t *currentFrontier = (uint64_t*)calloc(numNodes / 64, sizeof(uint64_t));	// Bit vector with one bit per node
	uint64_t *nextFrontier = (uint64_t*)calloc(numNodes / 64, sizeof(uint64_t));	// Bit vector with one bit per node
	setBit(nextFrontier[0], 0);	// Initialize frontier to first node
	uint32_t level = 1;

	// Partition data structure across DPUs
	uint32_t numNodesPerDPU =
	    ROUND_UP_TO_MULTIPLE_OF_64((numNodes - 1) / numDPUs + 1);
	PRINT_INFO(p.verbosity >= 1, "Assigning %u nodes per DPU",
		   numNodesPerDPU);
	struct DPUParams dpuParams[numDPUs];
	uint32_t dpuParams_m[numDPUs];
	unsigned int dpuIdx = 0;
	unsigned int t0ini = 0;
	unsigned int t1ini = 0;
	unsigned int t2ini = 0;
	unsigned int t3ini = 0;
	DPU_FOREACH(dpu_set, dpu) {

		// Allocate parameters
		struct mram_heap_allocator_t allocator;
		init_allocator(&allocator);
		dpuParams_m[dpuIdx] =
		    mram_heap_alloc(&allocator, sizeof(struct DPUParams));

		// Find DPU's nodes
		uint32_t dpuStartNodeIdx = dpuIdx * numNodesPerDPU;
		uint32_t dpuNumNodes;
		if (dpuStartNodeIdx > numNodes) {
			dpuNumNodes = 0;
		} else if (dpuStartNodeIdx + numNodesPerDPU > numNodes) {
			dpuNumNodes = numNodes - dpuStartNodeIdx;
		} else {
			dpuNumNodes = numNodesPerDPU;
		}
		dpuParams[dpuIdx].dpuNumNodes = dpuNumNodes;
		PRINT_INFO(p.verbosity >= 2, "    DPU %u:", dpuIdx);
		PRINT_INFO(p.verbosity >= 2, "        Receives %u nodes",
			   dpuNumNodes);

		// Partition edges and copy data
		if (dpuNumNodes > 0) {

			// Find DPU's CSR graph partition
			uint32_t *dpuNodePtrs_h = &nodePtrs[dpuStartNodeIdx];
			uint32_t dpuNodePtrsOffset = dpuNodePtrs_h[0];
			uint32_t *dpuNeighborIdxs_h =
			    neighborIdxs + dpuNodePtrsOffset;
			uint32_t dpuNumNeighbors =
			    dpuNodePtrs_h[dpuNumNodes] - dpuNodePtrsOffset;
			uint32_t *dpuNodeLevel_h = &nodeLevel[dpuStartNodeIdx];

			// Allocate MRAM
			uint32_t dpuNodePtrs_m =
			    mram_heap_alloc(&allocator,
					    (dpuNumNodes +
					     1) * sizeof(uint32_t));
			uint32_t dpuNeighborIdxs_m =
			    mram_heap_alloc(&allocator,
					    dpuNumNeighbors * sizeof(uint32_t));
			uint32_t dpuNodeLevel_m =
			    mram_heap_alloc(&allocator,
					    dpuNumNodes * sizeof(uint32_t));
			uint32_t dpuVisited_m =
			    mram_heap_alloc(&allocator,
					    numNodes / 64 * sizeof(uint64_t));
			uint32_t dpuCurrentFrontier_m =
			    mram_heap_alloc(&allocator,
					    dpuNumNodes / 64 *
					    sizeof(uint64_t));
			uint32_t dpuNextFrontier_m =
			    mram_heap_alloc(&allocator,
					    numNodes / 64 * sizeof(uint64_t));
			PRINT_INFO(p.verbosity >= 2,
				   "        Total memory allocated is %d bytes",
				   allocator.totalAllocated);

			// Set up DPU parameters
			dpuParams[dpuIdx].numNodes = numNodes;
			dpuParams[dpuIdx].dpuStartNodeIdx = dpuStartNodeIdx;
			dpuParams[dpuIdx].dpuNodePtrsOffset = dpuNodePtrsOffset;
			dpuParams[dpuIdx].level = level;
			dpuParams[dpuIdx].dpuNodePtrs_m = dpuNodePtrs_m;
			dpuParams[dpuIdx].dpuNeighborIdxs_m = dpuNeighborIdxs_m;
			dpuParams[dpuIdx].dpuNodeLevel_m = dpuNodeLevel_m;
			dpuParams[dpuIdx].dpuVisited_m = dpuVisited_m;
			dpuParams[dpuIdx].dpuCurrentFrontier_m =
			    dpuCurrentFrontier_m;
			dpuParams[dpuIdx].dpuNextFrontier_m = dpuNextFrontier_m;

			// Send data to DPU
			PRINT_INFO(p.verbosity >= 2,
				   "        Copying data to DPU");
			startTimer(&timer, 2, t0ini++);

			DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuNodePtrs_m, (uint8_t *) dpuNodePtrs_h,
						ROUND_UP_TO_MULTIPLE_OF_8((dpuNumNodes + 1) * sizeof(uint32_t))));

			DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuNeighborIdxs_m, (uint8_t *) dpuNeighborIdxs_h,
						ROUND_UP_TO_MULTIPLE_OF_8(dpuNumNeighbors * sizeof(uint32_t))));

			DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuNodeLevel_m, (uint8_t *) dpuNodeLevel_h,
						ROUND_UP_TO_MULTIPLE_OF_8(dpuNumNodes * sizeof(uint32_t))));

			DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuVisited_m, (uint8_t *) visited,
						ROUND_UP_TO_MULTIPLE_OF_8(numNodes / 64 * sizeof(uint64_t))));

			DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuNextFrontier_m, (uint8_t *) nextFrontier,
						ROUND_UP_TO_MULTIPLE_OF_8(numNodes / 64 * sizeof(uint64_t))));

			// NOTE: No need to copy current frontier because it is written before being read
			stopTimer(&timer, 2);
			//loadTime += getElapsedTime(timer);

		}
		// Send parameters to DPU
		PRINT_INFO(p.verbosity >= 2,
			   "        Copying parameters to DPU");
		startTimer(&timer, 2, t1ini++);
		DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
					dpuParams_m[dpuIdx], (uint8_t *) & dpuParams[dpuIdx],
					ROUND_UP_TO_MULTIPLE_OF_8(sizeof(struct DPUParams))));
		stopTimer(&timer, 2);
		//loadTime += getElapsedTime(timer);

		++dpuIdx;

	}

	// Iterate until next frontier is empty
	uint32_t nextFrontierEmpty = 0;
	while (!nextFrontierEmpty) {

		PRINT_INFO(p.verbosity >= 1,
			   "Processing current frontier for level %u", level);

#if ENERGY
		DPU_ASSERT(dpu_probe_start(&probe));
#endif
		// Run all DPUs
		PRINT_INFO(p.verbosity >= 1, "    Booting DPUs");
		startTimer(&timer, 3, t2ini++);
		DPU_ASSERT(dpu_launch(dpu_set, DPU_SYNCHRONOUS));
		stopTimer(&timer, 3);
		//dpuTime += getElapsedTime(timer);
#if ENERGY
		DPU_ASSERT(dpu_probe_stop(&probe));
		double energy;
		DPU_ASSERT(dpu_probe_get
			   (&probe, DPU_ENERGY, DPU_AVERAGE, &energy));
		tenergy += energy;
#endif

		// Copy back next frontier from all DPUs and compute their union as the current frontier
		startTimer(&timer, 4, t3ini++);
		dpuIdx = 0;
		DPU_FOREACH(dpu_set, dpu) {
			uint32_t dpuNumNodes = dpuParams[dpuIdx].dpuNumNodes;
			if (dpuNumNodes > 0) {
				if (dpuIdx == 0) {
					DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME,
								dpuParams[dpuIdx].dpuNextFrontier_m,
								(uint8_t *) currentFrontier,
								ROUND_UP_TO_MULTIPLE_OF_8(numNodes / 64 * sizeof(uint64_t))));
				} else {
					DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME,
								dpuParams[dpuIdx].dpuNextFrontier_m,
								(uint8_t *) nextFrontier,
								ROUND_UP_TO_MULTIPLE_OF_8(numNodes / 64 * sizeof(uint64_t))));
					for (uint32_t i = 0; i < numNodes / 64;
					     ++i) {
						currentFrontier[i] |=
						    nextFrontier[i];
					}
				}
				++dpuIdx;
			}
		}

		// Check if the next frontier is empty, and copy data to DPU if not empty
		nextFrontierEmpty = 1;
		for (uint32_t i = 0; i < numNodes / 64; ++i) {
			if (currentFrontier[i]) {
				nextFrontierEmpty = 0;
				break;
			}
		}
		if (!nextFrontierEmpty) {
			++level;
			dpuIdx = 0;
			DPU_FOREACH(dpu_set, dpu) {
				uint32_t dpuNumNodes =
				    dpuParams[dpuIdx].dpuNumNodes;
				if (dpuNumNodes > 0) {
					// Copy current frontier to all DPUs (place in next frontier and DPU will update visited and copy to current frontier)
					DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
								dpuParams[dpuIdx].dpuNextFrontier_m,
								(uint8_t *) currentFrontier,
								ROUND_UP_TO_MULTIPLE_OF_8(numNodes / 64 * sizeof(uint64_t))));
					// Copy new level to DPU
					dpuParams[dpuIdx].level = level;
					DPU_ASSERT(dpu_copy_to(dpu, DPU_MRAM_HEAP_POINTER_NAME,
								dpuParams_m[dpuIdx], (uint8_t *) &dpuParams[dpuIdx],
								ROUND_UP_TO_MULTIPLE_OF_8(sizeof(struct DPUParams))));
					++dpuIdx;
				}
			}
		}
		stopTimer(&timer, 4);
		//hostTime += getElapsedTime(timer);

	}

	// Copy back node levels
	PRINT_INFO(p.verbosity >= 1, "Copying back the result");
	startTimer(&timer, 5, 0);
	dpuIdx = 0;
	DPU_FOREACH(dpu_set, dpu) {
		uint32_t dpuNumNodes = dpuParams[dpuIdx].dpuNumNodes;
		if (dpuNumNodes > 0) {
			uint32_t dpuStartNodeIdx = dpuIdx * numNodesPerDPU;
			DPU_ASSERT(dpu_copy_from(dpu, DPU_MRAM_HEAP_POINTER_NAME,
						dpuParams[dpuIdx].dpuNodeLevel_m,
						(uint8_t *) (nodeLevel + dpuStartNodeIdx),
						ROUND_UP_TO_MULTIPLE_OF_8(dpuNumNodes * sizeof(float))));
		}
		++dpuIdx;
	}
	stopTimer(&timer, 5);
	//retrieveTime += getElapsedTime(timer);
	//if(p.verbosity == 0) PRINT("CPU-DPU Time(ms): %f    DPU Kernel Time (ms): %f    Inter-DPU Time (ms): %f    DPU-CPU Time (ms): %f", loadTime*1e3, dpuTime*1e3, hostTime*1e3, retrieveTime*1e3);

	// Calculating result on CPU
	PRINT_INFO(p.verbosity >= 1, "Calculating result on CPU");
	uint32_t *nodeLevelReference = (uint32_t*) calloc(numNodes, sizeof(uint32_t));	// Node's BFS level (initially all 0 meaning not reachable)
	memset(nextFrontier, 0, numNodes / 64 * sizeof(uint64_t));
	setBit(nextFrontier[0], 0);	// Initialize frontier to first node
	nextFrontierEmpty = 0;
	level = 1;
	startTimer(&timer, 6, 0);
	while (!nextFrontierEmpty) {
		// Update current frontier and visited list based on the next frontier from the previous iteration
		for (uint32_t nodeTileIdx = 0; nodeTileIdx < numNodes / 64;
		     ++nodeTileIdx) {
			uint64_t nextFrontierTile = nextFrontier[nodeTileIdx];
			currentFrontier[nodeTileIdx] = nextFrontierTile;
			if (nextFrontierTile) {
				visited[nodeTileIdx] |= nextFrontierTile;
				nextFrontier[nodeTileIdx] = 0;
				for (uint32_t node = nodeTileIdx * 64;
				     node < (nodeTileIdx + 1) * 64; ++node) {
					if (isSet(nextFrontierTile, node % 64)) {
						nodeLevelReference[node] =
						    level;
					}
				}
			}
		}
		// Visit neighbors of the current frontier
		nextFrontierEmpty = 1;
		for (uint32_t nodeTileIdx = 0; nodeTileIdx < numNodes / 64;
		     ++nodeTileIdx) {
			uint64_t currentFrontierTile =
			    currentFrontier[nodeTileIdx];
			if (currentFrontierTile) {
				for (uint32_t node = nodeTileIdx * 64;
				     node < (nodeTileIdx + 1) * 64; ++node) {
					if (isSet(currentFrontierTile, node % 64)) {	// If the node is in the current frontier
						// Visit its neighbors
						uint32_t nodePtr =
						    nodePtrs[node];
						uint32_t nextNodePtr =
						    nodePtrs[node + 1];
						for (uint32_t i = nodePtr;
						     i < nextNodePtr; ++i) {
							uint32_t neighbor =
							    neighborIdxs[i];
							if (!isSet(visited[neighbor / 64], neighbor % 64)) {	// Neighbor not previously visited
								// Add neighbor to next frontier
								setBit
								    (nextFrontier
								     [neighbor /
								      64],
								     neighbor %
								     64);
								nextFrontierEmpty
								    = 0;
							}
						}
					}
				}
			}
		}
		++level;
	}
	stopTimer(&timer, 6);

#if WITH_FREE_OVERHEAD
	startTimer(&timer, 7);
#endif
	DPU_ASSERT(dpu_free(dpu_set));
#if WITH_FREE_OVERHEAD
	stopTimer(&timer, 7);
#else
	zeroTimer(&timer, 7);
#endif

	// Verify the result
	PRINT_INFO(p.verbosity >= 1, "Verifying the result");
	int status = 1;
	for (uint32_t nodeIdx = 0; nodeIdx < numNodes; ++nodeIdx) {
		if (nodeLevel[nodeIdx] != nodeLevelReference[nodeIdx]) {
			PRINT_ERROR
			    ("Mismatch at node %u (CPU result = level %u, DPU result = level %u)",
			     nodeIdx, nodeLevelReference[nodeIdx],
			     nodeLevel[nodeIdx]);
			status = 0;
		}
	}

	if (status) {
		dfatool_printf
		    ("[::] BFS-UMEM | n_dpus=%d n_ranks=%d n_tasklets=%d e_type=%s n_elements=%d "
		     "| throughput_pim_MBps=%f throughput_MBps=%f", numDPUs, numRanks,
		     NR_TASKLETS, "uint32_t", numNodes,
		     numNodes * sizeof(uint32_t) / (timer.time[2] +
						    timer.time[3]),
		     numNodes * sizeof(uint32_t) / (timer.time[0] +
						    timer.time[1] +
						    timer.time[2] +
						    timer.time[3] +
						    timer.time[4]));
		dfatool_printf(" throughput_pim_MOpps=%f throughput_MOpps=%f",
		       numNodes / (timer.time[2] + timer.time[3]),
		       numNodes / (timer.time[0] + timer.time[1] +
				   timer.time[2] + timer.time[3] +
				   timer.time[4]));
		dfatool_printf
		    (" latency_alloc_us=%f latency_load_us=%f latency_write_us=%f latency_kernel_us=%f latency_sync_us=%f latency_read_us=%f latency_cpu_us=%f latency_free_us=%f\n",
		     timer.time[0], timer.time[1], timer.time[2], timer.time[3],
		     timer.time[4], timer.time[5], timer.time[6],
		     timer.time[7]);
	}
	// Display DPU Logs
	if (p.verbosity >= 2) {
		PRINT_INFO(p.verbosity >= 2, "Displaying DPU Logs:");
		dpuIdx = 0;
		DPU_FOREACH(dpu_set, dpu) {
			PRINT("DPU %u:", dpuIdx);
			DPU_ASSERT(dpu_log_read(dpu, stdout));
			++dpuIdx;
		}
	}
	// Deallocate data structures
	freeCOOGraph(cooGraph);
	freeCSRGraph(csrGraph);
	free(nodeLevel);
	free(visited);
	free(currentFrontier);
	free(nextFrontier);
	free(nodeLevelReference);

	return 0;

}
