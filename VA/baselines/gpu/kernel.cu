/* File:     vec_add.cu
 * Purpose:  Implement vector addition on a gpu using cuda
 *
 * Compile:  nvcc [-g] [-G] -o vec_add vec_add.cu
 * Run:      ./vec_add
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

__global__ void Vec_add(unsigned int x[], unsigned int y[], unsigned int z[], unsigned long long n) {
    unsigned long long thread_id = blockIdx.x * blockDim.x + threadIdx.x;
    if (thread_id < n){
        z[thread_id] = x[thread_id] + y[thread_id];
    }
}

cudaError_t Vec_add(unsigned long block_size, unsigned long num_threads, unsigned int x[], unsigned int y[], unsigned int z[], unsigned long long n) {
    Vec_add<<<block_size, num_threads>>>(x, y, z, n);
    return cudaGetLastError();
}
