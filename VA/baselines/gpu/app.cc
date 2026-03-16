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

#include <cuda.h>
#include <cuda_runtime.h>

#define CUDA_CHECK()                                                                                                     \
    if(cudaStatus != cudaSuccess) {                                                                                    \
        fprintf(stderr, "CUDA error: %s at %s:%d\n", cudaGetErrorString(cudaStatus), __FILE__, __LINE__);           \
        exit(-1);                                                                                                      \
    }

cudaError_t Vec_add(unsigned long block_size, unsigned long num_threads, unsigned int x[], unsigned int y[], unsigned int z[], unsigned long long n);

int main(int argc, char* argv[]) {
    unsigned long long n, m;
    unsigned int *h_x, *h_y, *h_z;
    unsigned int *d_x, *d_y, *d_z;
    unsigned long long size;
    cudaError_t  cudaStatus;

    /* Define vector length */
    n = 2621440;
    m = 320;

    if (argc >= 3) {
        n = atoll(argv[1]);
        m = atoll(argv[2]);
    }

    size = m * n * sizeof(unsigned int);

#if ASPECTC
    printf("[>>] VA | n_threads=%u n_rows=%lld\n", NR_THREADS, n * m);
#endif


    // Allocate memory for the vectors on host memory.
    h_x = (unsigned int*) malloc(size);
    h_y = (unsigned int*) malloc(size);
    h_z = (unsigned int*) malloc(size);

    for (unsigned long long i = 0; i < n * m; i++) {
        h_x[i] = i+1;
        h_y[i] = n-i;
    }

    // Print original vectors.
    /*printf("h_x = ");
    for (int i = 0; i < m; i++){
        printf("%u ", h_x[i]);
    }
    printf("\n\n");
    printf("h_y = ");
    for (int i = 0; i < m; i++){
        printf("%u ", h_y[i]);
    }
    printf("\n\n");*/

    /* Allocate vectors in device memory */
    cudaStatus = cudaMalloc(&d_x, size);
    CUDA_CHECK();
    cudaStatus = cudaMalloc(&d_y, size);
    CUDA_CHECK();
    cudaStatus = cudaMalloc(&d_z, size);
    CUDA_CHECK();

    /* Copy vectors from host memory to device memory */
    cudaStatus = cudaMemcpy(d_x, h_x, size, cudaMemcpyHostToDevice);
    CUDA_CHECK();
    cudaStatus = cudaMemcpy(d_y, h_y, size, cudaMemcpyHostToDevice);
    CUDA_CHECK();

    /* Kernel Call */
    cudaStatus = Vec_add((n * m) / NR_THREADS, NR_THREADS, d_x, d_y, d_z, n * m);
    CUDA_CHECK();

    cudaStatus = cudaMemcpy(h_z, d_z, size, cudaMemcpyDeviceToHost);
    CUDA_CHECK();
    /*printf("The sum is: \n");
    for (int i = 0; i < m; i++){
        printf("%u ", h_z[i]);
    }
    printf("\n");*/


    /* Free device memory */
    cudaFree(d_x);
    cudaFree(d_y);
    cudaFree(d_z);
    /* Free host memory */
    free(h_x);
    free(h_y);
    free(h_z);

#if ASPECTC
    printf("[<<] VA | n_threads=%u n_rows=%lld\n", NR_THREADS, n * m);
#endif

    return 0;
}  /* main */
