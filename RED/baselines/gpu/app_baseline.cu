/*
* JGL@SAFARI
*/

/**
* GPU code with Thrust
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <math.h>
#include <sys/time.h>

#include <vector>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/scan.h>
#include <thrust/copy.h>

#include "../../include/common.h"
#include "../../include/timer.h"

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

// Pointer declaration
static T* A;
static T* C;
static T* C2;

/**
* @brief creates input arrays
* @param nr_elements how many elements in input arrays
*/
static void read_input(T* A, unsigned int nr_elements) {
    //srand(0);
    for (unsigned int i = 0; i < nr_elements; i++) {
        //A[i] = (T) (rand()) % 2;
        A[i] = i;
    }
}

/**
* @brief compute output in the host
*/
static T reduction_host(T* A, unsigned int nr_elements) {
    T count = 0;
    for (unsigned int i = 0; i < nr_elements; i++) {
        count += A[i];
    }
    return count;
}

// Params ---------------------------------------------------------------------
typedef struct Params {
    unsigned int   input_size;
    int   n_warmup;
    int   n_reps;
    int   exp;
    int   n_threads;
}Params;

void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -w <W>    # of untimed warmup iterations (default=1)"
        "\n    -e <E>    # of timed repetition iterations (default=3)"
        "\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
        "\n    -t <T>    # of threads (default=8)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8M elements)"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 160 * 6553600;
    p.n_warmup      = 0;
    p.n_reps        = 1;
    p.exp           = 0;
    p.n_threads     = 8;

    int opt;
    while((opt = getopt(argc, argv, "hi:w:e:x:t:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
        case 'x': p.exp           = atoi(optarg); break;
        case 't': p.n_threads     = atoi(optarg); break;
        default:
            fprintf(stderr, "\nUnrecognized option!\n");
            usage();
            exit(0);
        }
    }
    assert(p.n_threads > 0 && "Invalid # of threads!");

    return p;
}

/**
* @brief Main of the Host Application.
*/
int main(int argc, char **argv) {

    cudaDeviceProp device_properties;
    cudaGetDeviceProperties(&device_properties, 0);
    cudaSetDevice(0);

    struct Params p = input_params(argc, argv);

    unsigned int nr_of_dpus = 1;
    
    const unsigned int input_size = p.exp == 0 ? p.input_size * nr_of_dpus : p.input_size;

    printf("[>>] RED | n_elements=%u\n", input_size);

    // Input/output allocation
    A = (T*)malloc(input_size * sizeof(T));
    C = (T*)malloc(input_size * sizeof(T));
    C2 = (T*)malloc(input_size * sizeof(T));

    T count = 0;
    T count_host = 0;

    // Create an input file with arbitrary data.
    read_input(A, input_size);

    thrust::host_vector<T> h_output(input_size);

	float time_ms;
	cudaEvent_t ev_start;
	cudaEvent_t ev_stop;
	cudaEventCreate(&ev_start);
	cudaEventCreate(&ev_stop);

    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        count_host = reduction_host(A, input_size);


        thrust::device_vector<T> d_input(input_size);
		cudaEventRecord(ev_start, 0);
        cudaMemcpy(thrust::raw_pointer_cast(&d_input[0]), A, input_size * sizeof(T), cudaMemcpyHostToDevice);
		cudaEventRecord(ev_stop, 0);
		cudaEventSynchronize(ev_stop);
		cudaEventElapsedTime(&time_ms, ev_start, ev_stop);
		printf("[::] cudaMemcpyHostToDevice @ %s:%d | payload_B=%lu | latency_ms=%f\n",
			__FILE__,
			__LINE__,
			input_size * sizeof(T),
			time_ms
		);

		cudaEventRecord(ev_start, 0);
        count = thrust::reduce(d_input.begin(),d_input.end());
		cudaEventRecord(ev_stop, 0);
		cudaEventSynchronize(ev_stop);
		cudaEventElapsedTime(&time_ms, ev_start, ev_stop);
		printf("[::] thrust::reduce @ %s:%d | n_elements=%u | latency_ms=%f\n",
			__FILE__,
			__LINE__,
			input_size * sizeof(T),
			input_size
		);

        h_output = d_input;
    }

    // Check output
    bool status = true;
    if(count_host != count){ 
        status = false;
        printf("%lu -- %lu\n", count, count_host);
    }
	if (status) {
        printf("[" ANSI_COLOR_GREEN "OK" ANSI_COLOR_RESET "] Outputs are equal\n");
    } else {
        printf("[" ANSI_COLOR_RED "ERROR" ANSI_COLOR_RESET "] Outputs differ!\n");
		return 1;
    }

    printf("[<<] RED | n_elements=%u\n", input_size);

    // Deallocation
    free(A);
    free(C);
    free(C2);
	
    return 0;
}
