#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../../include/common.h"

#include <cuda.h>
#include <cuda_runtime.h>

#define CUDA_CHECK()                                                                                                     \
    if(cudaStatus != cudaSuccess) {                                                                                    \
        fprintf(stderr, "CUDA error: %s at %s:%d\n", cudaGetErrorString(cudaStatus), __FILE__, __LINE__);           \
        exit(-1);                                                                                                      \
    }

cudaError_t gemv(unsigned int numBlocks, unsigned int numThreadsPerBlock, int m, int n, T* adim, T* b, T* d_ans);

void cgemv(int m, int n, T *adim, T *b, T *d_ans);

int main(int argc, char **argv)
{
	/* for CPU */
	int i, j;
	T **bdim;
	T *c, *ans, *h_ans, *h_c;
	int n = 8192;
	int m = 20480;

	if (argc >= 3) {
		n = atoi(argv[1]);
		m = atoi(argv[2]);
	}

#if ASPECTC
	printf("[>>] MLP | n_gpu_threads=%d n=%d m=%d\n", NR_THREADS, n, m);
#endif

    cudaError_t cudaStatus;

	bdim = (T**) malloc(NUM_LAYERS * sizeof(T*));
	for(int l = 0; l < NUM_LAYERS; l++)
		bdim[l] = (T*)malloc(sizeof(T)*m*n);
	c = (T*)malloc(sizeof(T) *n);
	h_c = (T*)malloc(sizeof(T) *n);
	ans = (T*)malloc(sizeof(T) *m);
	h_ans = (T*)malloc(sizeof(T) *m);

	/* for GPU */
	T *d_bdim; 
	T *d_c, *d_ans;
	cudaStatus = cudaMalloc((void **)&d_bdim, sizeof(T)*m*n);
	CUDA_CHECK();
	cudaStatus = cudaMalloc((void **)&d_c, sizeof(T)*n);
	CUDA_CHECK();
	cudaStatus = cudaMalloc((void **)&d_ans, sizeof(T)*m);
	CUDA_CHECK();

	for(i = 0; i < n; i++)
	{
		if(i % 50 < 48)
		{
			c[i] = 0;
			h_c[i] = 0;
		}
		else
		{
			c[i] = i % 2;
			h_c[i] = i % 2;
		}
	}
	for(int l = 0; l < NUM_LAYERS; l++)
		for(i = 0; i < n; i++)
		{
			for(j = 0; j < m; j++){
				if(j % 100 < 98)
				{

					bdim[l][i*m+j] = 0;
				}
				else
				{

					bdim[l][i*m+j] = (l + i) % 2;
				}
			}
		}

	for(j = 0; j < m; j++){
		ans[j] = 0;
		h_ans[j] = 0;
	}
	// Computation on the host for verification
	T* vector = c;
	T* output = ans;
	T* matrix;
	int mm = m;
	int nn = n;
	for(int l = 0; l < NUM_LAYERS; l++){
		matrix = bdim[l];
		cgemv(mm, nn, matrix, vector, output);
		vector = output;
                h_ans = output;
		mm = n; nn = m;
	}

	// Event creation
	cudaStatus = cudaMemcpy(d_ans, h_ans, sizeof(T)*m, cudaMemcpyHostToDevice);
	CUDA_CHECK();
	cudaStatus = cudaMemcpy(d_c, h_c, sizeof(T)*n, cudaMemcpyHostToDevice);
	CUDA_CHECK();

	vector = d_c;
	output = d_ans;
	mm = m;
	nn = n;
	for(int l = 0; l < NUM_LAYERS; l++){
		cudaStatus = cudaMemcpy(d_bdim, bdim[l], sizeof(T)*m*n, cudaMemcpyHostToDevice);
		CUDA_CHECK();
		matrix = d_bdim;
		cudaStatus = gemv(mm, NR_THREADS, mm, nn, matrix, vector, output);
		CUDA_CHECK();
		vector = output;
		d_ans = output;
		mm = n; nn = m;
	}

	cudaStatus = cudaMemcpy(h_ans, d_ans, sizeof(T)*m, cudaMemcpyDeviceToHost);
	CUDA_CHECK();
	cudaStatus = cudaMemcpy(h_c, d_c, sizeof(T)*n, cudaMemcpyDeviceToHost);
	CUDA_CHECK();

	for(i = 0; i < m; i++)
	{
		if(ans[i] != h_ans[i]) {
			printf("ERROR in Ans %d -> %d -- %d\n", i, ans[i], h_ans[i]);
			return 1;
		}
	}

	for(i = 0; i < n; i++)
	{
		if(c[i] != h_c[i]) {
			printf("ERROR in C %d -> %d -- %d\n", i, c[i], h_c[i]);
			return 1;
		}
	}

#if ASPECTC
	printf("[<<] MLP | n_gpu_threads=%d n=%d m=%d\n", NR_THREADS, n, m);
#endif


	for(int l = 0; l < NUM_LAYERS; l++)
		free(bdim[l]);


	free(bdim);
	free(c);
	free(ans);
	free(h_c);
	cudaFree(d_bdim);
	cudaFree(d_c);
	cudaFree(d_ans);

	return 0;
} 

void cgemv(int m, int n, T *adim, T *b, T *d_ans)
{
	int i, j;

	for(i = 0; i < m; i++){
		for(j = 0; j < n; j++)
			d_ans[i] += adim[i*n+j] * b[j];
		d_ans[i] = max(0, d_ans[i]);
	}

}
