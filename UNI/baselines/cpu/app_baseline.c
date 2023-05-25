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

#define XSTR(x) STR(x)
#define STR(x) #x

#ifndef T
#define T int64_t
#endif

static int pos;

static T *A;
static T *B;
static T *C;
static T *C2;

// Create a "test file"
static T *create_test_file(unsigned int nr_elements) {
    //srand(0);

    A = (T*) malloc(nr_elements * sizeof(T));
    B = (T*) malloc(nr_elements * sizeof(T));
    C = (T*) malloc(nr_elements * sizeof(T));

    printf("nr_elements\t%u\t", nr_elements);
    for (int i = 0; i < nr_elements; i++) {
        //A[i] = (unsigned int) (rand());
        //A[i] = i+1;
        //A[i] = i%2==0?i+1:i;
        A[i] = i%2==0?i:i+1;
        B[i] = 0;
    }

    return A;
}

// Compute output in the host
static int unique_host(int size, int t) {
    pos = 0;
    C[pos] = A[pos];

    omp_set_num_threads(t);
    #pragma omp parallel for
    for(int my = 1; my < size; my++) {
        if(A[my] != A[my-1]) {
            int p;
            #pragma omp atomic update
            pos++;
            p = pos;
            C[p] = A[my];
        }
    }

    return pos;
}

// Params 
typedef struct Params {
    int   input_size;
    int   n_warmup;
    int   n_reps;
    int   n_threads;
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
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8M elements)"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 16 << 20;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.n_threads     = 8;

    int opt;
    while((opt = getopt(argc, argv, "hd:i:w:e:t:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
        case 't': p.n_threads     = atoi(optarg); break;
        default:
            fprintf(stderr, "\nUnrecognized option!\n");
            usage();
            exit(0);
        }
    }
    assert(p.n_threads > 0 && "Invalid # of ranks!");

    return p;
}

// Main
int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);

    const unsigned int file_size = p.input_size;
    uint32_t accum = 0;
    int total_count;

    // Create an input file with arbitrary data
    create_test_file(file_size);

    Timer timer;

    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {
        start(&timer, 0, 0);
        total_count = unique_host(file_size, p.n_threads);
        stop(&timer, 0);

        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        if (rep >= p.n_warmup) {
            printf("[::] n_threads=%d e_type=%s n_elements=%d "
                "| throughput_cpu_MBps=%f\n",
                nr_threads, XSTR(T), file_size,
                file_size * 2 * sizeof(T) / timer.time[0]);
            printf("[::] n_threads=%d e_type=%s n_elements=%d "
                "| throughput_cpu_MOpps=%f\n",
                nr_threads, XSTR(T), file_size,
                file_size / timer.time[0]);
            printf("[::] n_threads=%d e_type=%s n_elements=%d | ",
                nr_threads, XSTR(T), file_size);
            printall(&timer, 0);
        }
    }

    free(A);
    free(B);
    free(C);
    return 0;
  }
