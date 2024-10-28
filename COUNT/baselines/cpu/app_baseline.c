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
#define T uint64_t
#endif

volatile int total_count;

// Params ---------------------------------------------------------------------
typedef struct Params {
    char* dpu_type;
    int   input_size;
    int   n_warmup;
    int   n_reps;
    int   n_threads;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif
}Params;

struct Params p;

static T *A;

bool pred(const T x){
  return (x % 2) == 0;
}


void create_test_file(unsigned int nr_elements) {
    //srand(0);

#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
    A = (T*) numa_alloc(nr_elements * sizeof(T));
#else
    A = (T*) malloc(nr_elements * sizeof(T));
#endif

#if NUMA
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif

    for (unsigned int i = 0; i < nr_elements; i++) {
        //A[i] = (unsigned int) (rand());
        A[i] = i+1;
    }

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

    numa_node_cpu = p.numa_node_cpu;
    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#endif

}

/**
* @brief compute output in the host
*/
static int count_host(int size, int t) {
    int count = 0;

    omp_set_num_threads(t);
    #pragma omp parallel for reduction(+:count)
    for(int my = 0; my < size; my++) {
        if(!pred(A[my])) {
            count++;
        }
    }
    return count;
}

void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -d <D>    DPU type (default=fsim)"
        "\n    -t <T>    # of threads (default=8)"
        "\n    -w <W>    # of untimed warmup iterations (default=2)"
        "\n    -e <E>    # of timed repetition iterations (default=5)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8M elements)"
        "\n");
}

void input_params(int argc, char **argv) {
    p.input_size    = 16 << 20;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.n_threads     = 5;
#if NUMA
    p.bitmask_in     = NULL;
    p.bitmask_out    = NULL;
    p.numa_node_cpu = -1;
#endif

    int opt;
    while((opt = getopt(argc, argv, "hi:w:e:t:a:b:c:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
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
}

/**
* @brief Main of the Host Application.
*/
int main(int argc, char **argv) {

    input_params(argc, argv);

    const unsigned int file_size = p.input_size;

    // Create an input file with arbitrary data.
    create_test_file(file_size);

    Timer timer;

    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
        start(&timer, 0, 0);
        total_count = count_host(file_size, p.n_threads);
        stop(&timer, 0);

        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        if (rep >= p.n_warmup) {
            printf("[::] COUNT-CPU | n_threads=%d e_type=%s n_elements=%d"
#if NUMA
                " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
                " | throughput_MBps=%f",
                nr_threads, XSTR(T), file_size,
#if NUMA
                numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
                file_size * 2 * sizeof(T) / timer.time[0]);
            printf(" throughput_MOpps=%f",
                file_size / timer.time[0]);
            printall(&timer, 0);
        }
    }

#if NUMA
    numa_free(A, file_size * sizeof(T));
#else
    free(A);
#endif
    return 0;
}
