#include <cuda.h>
#include <limits.h>
#include "binary_search.h"

#include <chrono>
#include <iostream>

#define BLOCKDIM 512
#define SEARCH_CHUNK 16
#define BLOCK_CHUNK (BLOCKDIM*SEARCH_CHUNK)


__global__ void _search_kernel(const long int *arr,
    const long int len, const long int *querys, const long int num_querys, long int *res, bool *flag)
{
    int search;
    if(*flag == false) {
        int tid = threadIdx.x;
        __shared__ int s_arr[BLOCK_CHUNK];

        /* Since each value is being copied to shared memory, the rest of the
        following uncommented code is unncessary, since a direct comparison
        can be done at the time of copy below. */
        // for(int i = 0; i < BLOCKDIM; ++i) {
        //     int shared_loc = i*SEARCH_CHUNK + tid;
        //     int global_loc = shared_loc + BLOCK_CHUNK * blockIdx.x;
        //     if(arr[global_loc] == search) {
        //         *flag = true;
        //         *res = global_loc;
        //     }
        //     __syncthreads();
        // }

        /* Copy chunk of array that this entire block of threads will read
        from the slower global memory to the faster shared memory. */
        for(long int i = 0; i < SEARCH_CHUNK; ++i) {
            int shared_loc = tid*SEARCH_CHUNK + i;
            int global_loc = shared_loc + BLOCK_CHUNK * blockIdx.x;

            /* Make sure to stay within the bounds of the global array,
            else assign a dummy value. */
            if(global_loc < len) {
              s_arr[shared_loc] = arr[global_loc];
            }
            else {
              s_arr[shared_loc] = INT_MAX;
            }
        }
        __syncthreads();

        for(long int i = 0; i < num_querys; i++)
        {
            search = querys[i];
            /* For each thread, set the initial search range. */
            int L = 0;
            int R = SEARCH_CHUNK - 1;
            int m = (L + R) / 2;

            /* Pointer to the part of the shared array for this thread. */
            int *s_ptr = &s_arr[tid*SEARCH_CHUNK];

            /* Each thread will search a chunk of the block array.
            Many blocks will not find a solution so the search must
            be allowed to fail on a per block basis. The loop will
            break (fail) when L >= R. */
            while(L <= R && *flag == false)
            {
                if(s_ptr[m] < search) {
                    L = m + 1;
                }
                else if(s_ptr[m] > search) {
                    R = m - 1;
                }
                else {
                    *flag = true;
                    *res = m += tid*SEARCH_CHUNK + BLOCK_CHUNK * blockIdx.x;
                }

                m = (L + R) / 2;
            }
        }
    }
}


void search_kernel(unsigned long int gridSize, unsigned long int blockSize, const long int *arr,
    const long int len, const long int *querys, const long int num_querys, long int *res, bool *flag)
{
    _search_kernel<<<gridSize,blockSize>>>(arr, len, querys, num_querys ,res, flag);
}
