#include <stdio.h>
#include <stdlib.h>
#include <cuda.h>
#include "../../include/common.h"

__global__ void _gemv(int m, int n, T* adim, T* b, T* d_ans)
{
	int i;
	int div = n/NR_THREADS;
	__shared__ T tmp[NR_THREADS];

	tmp[threadIdx.x] = 0.0;

	for(i = 0; i < div; i++){
		tmp[threadIdx.x] += adim[blockIdx.x*n+i*NR_THREADS+threadIdx.x] * b[i * NR_THREADS + threadIdx.x];
	}
	if(threadIdx.x < m%NR_THREADS)
		tmp[threadIdx.x] += adim[blockIdx.x*n+NR_THREADS*div+threadIdx.x] * b[NR_THREADS * div + threadIdx.x];

	__syncthreads();

	for(i = NR_THREADS / 2; i > 31; i = i / 2)
	{
		if(threadIdx.x < i)
			tmp[threadIdx.x] += tmp[threadIdx.x + i];
		__syncthreads();
	}

	if(threadIdx.x < 16)
	{
		tmp[threadIdx.x] += tmp[threadIdx.x + 16];
		__syncthreads();
		tmp[threadIdx.x] += tmp[threadIdx.x + 8];
		__syncthreads();
		tmp[threadIdx.x] += tmp[threadIdx.x + 4];
		__syncthreads();
		tmp[threadIdx.x] += tmp[threadIdx.x + 2];
		__syncthreads();
		tmp[threadIdx.x] += tmp[threadIdx.x + 1];
		__syncthreads();
	}


	if(threadIdx.x == 0)
		d_ans[blockIdx.x] = max(0, tmp[0]);

}

cudaError_t gemv(unsigned int numBlocks, unsigned int numThreadsPerBlock, int m, int n, T* adim, T* b, T* d_ans) {
	_gemv<<<numBlocks, numThreadsPerBlock>>>(m, n, adim, b, d_ans);
	return cudaGetLastError();
}
