/**
* @file app.c
* @brief Template for a Host Application Source File.
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <stdint.h>

#include <omp.h>
#include "../../support/timer.h"

#if NUMA
#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef T
#define T int32_t
#endif

static T *A;
static T *B;
static T *C;

/**
* @brief compute output in the host
*/
static void vector_addition_host(unsigned int nr_elements, int t) {
    omp_set_num_threads(t);
    #pragma omp parallel for
    for (int i = 0; i < nr_elements; i++) {
        C[i] = A[i] + B[i];
    }
}

// Params ---------------------------------------------------------------------
typedef struct Params {
    int   input_size;
    int   n_warmup;
    int   n_reps;
    int   exp;
    int   n_threads;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif
}Params;

void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -t <T>    # of threads (default=8)"
        "\n    -w <W>    # of untimed warmup iterations (default=1)"
        "\n    -e <E>    # of timed repetition iterations (default=3)"
        "\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8M elements)"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 16777216;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.exp           = 1;
    p.n_threads     = 5;
#if NUMA
    p.bitmask_in     = NULL;
    p.bitmask_out    = NULL;
    p.numa_node_cpu = -1;
#endif

    int opt;
    while((opt = getopt(argc, argv, "hi:w:e:x:t:a:b:c:")) >= 0) {
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
#if NUMA
        case 'a': p.bitmask_in    = numa_parse_nodestring(optarg); break;
        case 'b': p.bitmask_out   = numa_parse_nodestring(optarg); break;
        case 'c': p.numa_node_cpu = atoi(optarg); break;
#endif
        default:
            fprintf(stderr, "\nUnrecognized option!\n");
            usage();
            exit(0);
        }
    }
    assert(p.n_threads > 0 && "Invalid # of ranks!");

    return p;
}

/**
* @brief Main of the Host Application.
*/
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    const unsigned int input_size = p.exp == 0 ? p.input_size * p.n_threads : p.input_size;

    // Create an input file with arbitrary data.
    /**
    * @brief creates a "test file" by filling a buffer of 64MB with pseudo-random values
    * @param nr_elements how many 32-bit elements we want the file to be
    * @return the buffer address
    */
    srand(0);

#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
    A = (T*) numa_alloc(input_size * sizeof(T));
    B = (T*) numa_alloc(input_size * sizeof(T));
#else
    A = (T*) malloc(input_size * sizeof(T));
    B = (T*) malloc(input_size * sizeof(T));
#endif

#if NUMA
    if (p.bitmask_out) {
        numa_set_membind(p.bitmask_out);
        numa_free_nodemask(p.bitmask_out);
    }
    C = (T*) numa_alloc(input_size * sizeof(T));
#else
    C = (T*) malloc(input_size * sizeof(T));
#endif

    for (unsigned int i = 0; i < input_size; i++) {
        A[i] = (T) (rand());
        B[i] = (T) (rand());
    }

#if NUMA
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif

#if NUMA
    mp_pages[0] = A;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_in = mp_status[0];
    }

    mp_pages[0] = C;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(C)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_out = mp_status[0];
    }

    numa_node_cpu = p.numa_node_cpu;
    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#endif

    Timer timer;

    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
        start(&timer, 0, 0);
        vector_addition_host(input_size, p.n_threads);
        stop(&timer, 0);

        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        if (rep >= p.n_warmup) {
            printf("[::] VA-CPU | n_threads=%d e_type=%s n_elements=%d"
#if NUMA
                " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
                " | latency_us=%f throughput_MBps=%f",
                nr_threads, XSTR(T), input_size,
#if NUMA
                numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
                timer.time[0], input_size * 3 * sizeof(T) / timer.time[0]);
            printf(" throughput_MOpps=%f",
                input_size / timer.time[0]);
            printall(&timer, 0);
        }
    }

#if NUMA
    numa_free(A, input_size * sizeof(T));
    numa_free(B, input_size * sizeof(T));
    numa_free(C, input_size * sizeof(T));
#else
    free(A);
    free(B);
    free(C);
#endif

   return 0;
 }
