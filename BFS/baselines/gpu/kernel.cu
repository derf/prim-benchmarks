#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/common.h"
#include "../../include/graph.h"
#include "../../include/params.h"
#include "../../include/utils.h"

__global__ void _bfs_kernel(CSRGraph csrGraph, uint32_t* nodeLevel,
    uint32_t* prevFrontier, uint32_t* currFrontier,
    uint32_t numPrevFrontier, uint32_t* numCurrFrontier,
    uint32_t level)
{
	uint32_t i = blockIdx.x * blockDim.x + threadIdx.x;
	if (i < numPrevFrontier) {
		uint32_t node = prevFrontier[i];
		for (uint32_t edge = csrGraph.nodePtrs[node];
		    edge < csrGraph.nodePtrs[node + 1]; ++edge) {
			uint32_t neighbor = csrGraph.neighborIdxs[edge];
			if (atomicCAS(&nodeLevel[neighbor], UINT32_MAX,
			        level)
			    == UINT32_MAX) { // Node not previously visited
				uint32_t currFrontierIdx = atomicAdd(numCurrFrontier, 1);
				currFrontier[currFrontierIdx] = neighbor;
			}
		}
	}
}

void bfs_kernel(uint32_t numBlocks, uint32_t numThreadsPerBlock,
    CSRGraph csrGraph_d, uint32_t* nodeLevel_d,
    uint32_t* prevFrontier_d, uint32_t* currFrontier_d,
    uint32_t numPrevFrontier, uint32_t* numCurrFrontier_d,
    uint32_t level)
{
	_bfs_kernel<<<numBlocks, numThreadsPerBlock>>>(
	    csrGraph_d, nodeLevel_d, prevFrontier_d, currFrontier_d,
	    numPrevFrontier, numCurrFrontier_d, level);
}
