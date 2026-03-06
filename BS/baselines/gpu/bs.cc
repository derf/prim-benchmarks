#include <cuda.h>
#include <cuda_runtime.h>
#include <limits.h>
#include "binary_search.h"
#include <cuda.h>
#include <cuda_runtime.h>

#include <chrono>
#include <iostream>

#define BLOCKDIM 512
#define SEARCH_CHUNK 16
#define BLOCK_CHUNK (BLOCKDIM*SEARCH_CHUNK)

void search_kernel(unsigned long int gridSize, unsigned long int blockSize, const long int *arr,
    const long int len, const long int *querys, const long int num_querys, long int *res, bool *flag);

int binary_search(const long int *arr, const long int len, const long int *querys, const long int num_querys)
{
    long int *d_arr, *d_querys, *d_res;
    bool *d_flag;

    size_t arr_size = len * sizeof(long int);
    size_t querys_size = num_querys * sizeof(long int);
    size_t res_size = sizeof(long int);
    size_t flag_size = sizeof(bool);

    cudaMalloc(&d_arr, arr_size);
    cudaMalloc(&d_querys, querys_size);
    cudaMalloc(&d_res, res_size);
    cudaMalloc(&d_flag, flag_size);

    cudaMemcpy(d_arr, arr, arr_size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_querys, querys, querys_size, cudaMemcpyHostToDevice);

    cudaMemset(d_flag, 0, flag_size);

    /* Set res value to -1, so that if the function returns -1, that
    indicates an algorithm failure. */
    cudaMemset(d_res, -0x1, res_size);

    int blockSize = BLOCKDIM;
    int gridSize = (len-1)/BLOCK_CHUNK + 1;

    search_kernel(gridSize,blockSize,d_arr, len, d_querys, num_querys ,d_res, d_flag);
    cudaDeviceSynchronize();

    long int res;

    cudaMemcpy(&res, d_res, res_size, cudaMemcpyDeviceToHost);

    return res;
}
