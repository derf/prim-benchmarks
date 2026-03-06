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

void Vec_add(unsigned long block_size, unsigned long num_threads, unsigned int x[], unsigned int y[], unsigned int z[], unsigned long long n);

int main(int argc, char* argv[]) {
    unsigned long long n, m;
    unsigned int *h_x, *h_y, *h_z;
    unsigned int *d_x, *d_y, *d_z;
    unsigned long long size;

    /* Define vector length */
    n = 2621440;
    m = 320;

    if (argc >= 3) {
        n = atoll(argv[1]);
        m = atoll(argv[2]);
    }

    size = m * n * sizeof(unsigned int);

#if ASPECTC
    printf("[>>] VA | n_rows=%lld\n", n * m);
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
    cudaMalloc(&d_x, size);
    cudaMalloc(&d_y, size);
    cudaMalloc(&d_z, size);

    /* Copy vectors from host memory to device memory */
    cudaMemcpy(d_x, h_x, size, cudaMemcpyHostToDevice);
    cudaMemcpy(d_y, h_y, size, cudaMemcpyHostToDevice);

    /* Kernel Call */
    Vec_add((n * m) / 256, 256, d_x, d_y, d_z, n * m);

    cudaMemcpy(h_z, d_z, size, cudaMemcpyDeviceToHost);
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
    printf("[<<] VA | n_rows=%lld\n", n * m);
#endif

    return 0;
}  /* main */
