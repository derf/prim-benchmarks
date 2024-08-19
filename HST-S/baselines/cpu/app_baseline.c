/*
* JGL@SAFARI
*/

/**
* @file app.c
* @brief Template for a Host Application Source File.
*
* The macros DPU_BINARY and NR_TASKLETS are directly
* used in the static functions, and are not passed as arguments of these functions.
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

#if NUMA
#include <numaif.h>
#include <numa.h>

struct bitmask* bitmask_in;
struct bitmask* bitmask_out;

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#if NUMA_MEMCPY
int numa_node_cpu_memcpy = -1;
int numa_node_local = -1;
int numa_node_in_is_local = 0;
#endif


#include "../../support/common.h"
#include "../../support/timer.h"

#define XSTR(x) STR(x)
#define STR(x) #x

// Pointer declaration
static T* A;
static T *A_local;
static unsigned int* histo_host;

typedef struct Params {
    unsigned int   input_size;
    unsigned int   bins;
    int   n_warmup;
    int   n_reps;
    const char *file_name;
    int  exp;
    int  n_threads;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif
#if NUMA_MEMCPY
    int numa_node_cpu_memcpy;
    struct bitmask* bitmask_cpu;
#endif
}Params;

/**
* @brief creates input arrays
* @param nr_elements how many elements in input arrays
*/
static void read_input(T* A, const Params p) {

    char  dctFileName[100];
    FILE *File = NULL;

    // Open input file
    unsigned short temp;
    sprintf(dctFileName, "%s", p.file_name);
    if((File = fopen(dctFileName, "rb")) != NULL) {
        for(unsigned int y = 0; y < p.input_size; y++) {
            if (fread(&temp, sizeof(unsigned short), 1, File) == 1) {
                A[y] = (unsigned int)ByteSwap16(temp);
                if(A[y] >= 4096)
                    A[y] = 4095;
            } else {
                //printf("out of bounds read at offset %d -- seeking back to 0\n", y);
                rewind(File);
            }
        }
        fclose(File);
    } else {
        printf("%s does not exist\n", dctFileName);
        exit(1);
    }
}

/**
* @brief compute output in the host
*/
static void histogram_host(unsigned int* histo, T* A, unsigned int bins, unsigned int nr_elements, int exp, unsigned int nr_of_dpus, int t) {

    omp_set_num_threads(t);

    if(!exp){
        #pragma omp parallel for
        for (unsigned int i = 0; i < nr_of_dpus; i++) {
            for (unsigned int j = 0; j < nr_elements; j++) {
                T d = A[j];
                histo[i * bins + ((d * bins) >> DEPTH)] += 1;
            }
        }
    }
    else{
        #pragma omp parallel for
        for (unsigned int j = 0; j < nr_elements; j++) {
            T d = A[j];
            #pragma omp atomic update
            histo[(d * bins) >> DEPTH] += 1;
        }
    }
}

// Params ---------------------------------------------------------------------
void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -w <W>    # of untimed warmup iterations (default=1)"
        "\n    -e <E>    # of timed repetition iterations (default=3)"
        "\n    -t <T>    # of threads (default=8)"
        "\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=1536*1024 elements)"
        "\n    -b <B>    histogram size (default=256 bins)"
        "\n    -f <F>    input image file (default=../input/image_VanHateren.iml)"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 1536 * 1024;
    p.bins          = 256;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.n_threads     = 8;
    p.exp           = 1;
    p.file_name     = "../../input/image_VanHateren.iml";
#if NUMA
    p.bitmask_in     = NULL;
    p.bitmask_out    = NULL;
    p.numa_node_cpu = -1;
#endif
#if NUMA_MEMCPY
    p.numa_node_cpu_memcpy  = -1;
    p.bitmask_cpu    = NULL;
#endif

    int opt;
    while((opt = getopt(argc, argv, "hi:b:w:e:f:x:t:A:B:C:D:M:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'b': p.bins          = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
        case 'f': p.file_name     = optarg; break;
        case 'x': p.exp           = atoi(optarg); break;
        case 't': p.n_threads     = atoi(optarg); break;
#if NUMA
        case 'A': p.bitmask_in    = numa_parse_nodestring(optarg); break;
        case 'B': p.bitmask_out   = numa_parse_nodestring(optarg); break;
        case 'C': p.numa_node_cpu = atoi(optarg); break;
#if NUMA_MEMCPY
        case 'D': p.bitmask_cpu   = numa_parse_nodestring(optarg); break;
        case 'M': p.numa_node_cpu_memcpy = atoi(optarg); break;
#endif // NUMA_MEMCPY
#endif // NUMA
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

    uint32_t nr_of_dpus;
    
    const unsigned int input_size = p.input_size; // Size of input image
    if(!p.exp)
        assert(input_size % p.n_threads == 0 && "Input size!");
    else
        assert(input_size % p.n_threads == 0 && "Input size!");

    // Input/output allocation
#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
    A = numa_alloc(input_size * sizeof(T));
#else
    A = malloc(input_size * sizeof(T));
#endif

    // Create an input file with arbitrary data.
    read_input(A, p);

#if NUMA
    if (p.bitmask_out) {
        numa_set_membind(p.bitmask_out);
        numa_free_nodemask(p.bitmask_out);
    }
#endif
    if(!p.exp) {
        // upstream code left nr_of_dpus uninitialized
        nr_of_dpus = p.n_threads;
#if NUMA
        histo_host = numa_alloc(nr_of_dpus * p.bins * sizeof(unsigned int));
#else
        histo_host = malloc(nr_of_dpus * p.bins * sizeof(unsigned int));
#endif
    } else {
#if NUMA
        histo_host = numa_alloc(p.bins * sizeof(unsigned int));
#else
        histo_host = malloc(p.bins * sizeof(unsigned int));
#endif
    }

#if NUMA
#if NUMA_MEMCPY
    if (p.bitmask_cpu) {
        numa_set_membind(p.bitmask_cpu);
        numa_free_nodemask(p.bitmask_cpu);
    }
#else
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif // NUMA_MEMCPY
#endif // NUMA

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

    mp_pages[0] = histo_host;
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

#if NUMA_MEMCPY
    numa_node_in_is_local = ((numa_node_cpu == numa_node_in) || (numa_node_cpu + 8 == numa_node_in)) * 1;
#endif

    Timer timer;

#if NUMA_MEMCPY
    numa_node_cpu_memcpy = p.numa_node_cpu_memcpy;
    start(&timer, 1, 0);
    if (!numa_node_in_is_local) {
        A_local = (T*) numa_alloc(input_size * sizeof(T));
    }
    stop(&timer, 1);
    if (!numa_node_in_is_local) {
        if (p.numa_node_cpu_memcpy != -1) {
            if (numa_run_on_node(p.numa_node_cpu_memcpy) == -1) {
                perror("numa_run_on_node");
                numa_node_cpu_memcpy = -1;
            }
        }
    }
    start(&timer, 2, 0);
    if (!numa_node_in_is_local) {
        memcpy(A_local, A, input_size * sizeof(T));
    } else {
        A_local = A;
    }
    stop(&timer, 2);
    if (p.numa_node_cpu != -1) {
        if (numa_run_on_node(p.numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
    mp_pages[0] = A_local;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A_local)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_local = mp_status[0];
    }
#else
    A_local = A;
#endif

    start(&timer, 0, 0);

    if(!p.exp)
        memset(histo_host, 0, nr_of_dpus * p.bins * sizeof(unsigned int));
    else
        memset(histo_host, 0, p.bins * sizeof(unsigned int));

    histogram_host(histo_host, A_local, p.bins, input_size, p.exp, nr_of_dpus, p.n_threads);

    stop(&timer, 0);

#if NUMA_MEMCPY
    start(&timer, 3, 0);
    if (!numa_node_in_is_local) {
        numa_free(A_local, input_size * sizeof(T));
    }
    stop(&timer, 3);
#endif

    unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
    nr_threads++;

#if NUMA_MEMCPY
    printf("[::] HST-S-CPU-MEMCPY | n_threads=%d e_type=%s n_elements=%d n_bins=%d"
        " numa_node_in=%d numa_node_local=%d numa_node_out=%d numa_node_cpu=%d numa_node_cpu_memcpy=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
        " | throughput_MBps=%f",
        nr_threads, XSTR(T), input_size, p.exp ? p.bins : p.bins * nr_of_dpus,
        numa_node_in, numa_node_local, numa_node_out, numa_node_cpu, numa_node_cpu_memcpy, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
        input_size * sizeof(T) / timer.time[0]);
    printf(" throughput_MOpps=%f",
        input_size / timer.time[0]);
    printf(" latency_kernel_us=%f latency_alloc_us=%f latency_memcpy_us=%f latency_free_us=%f latency_total_us=%f\n",
        timer.time[0], timer.time[1], timer.time[2], timer.time[3],
        timer.time[0] + timer.time[1] + timer.time[2] + timer.time[3]);
#else
    printf("[::] HST-S-CPU | n_threads=%d e_type=%s n_elements=%d n_bins=%d"
#if NUMA
        " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
        " | throughput_MBps=%f",
        nr_threads, XSTR(T), input_size, p.exp ? p.bins : p.bins * nr_of_dpus,
#if NUMA
        numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
        input_size * sizeof(T) / timer.time[0]);
    printf(" throughput_MOpps=%f latency_us=%f\n",
        input_size / timer.time[0], timer.time[0]);
#endif // NUMA_MEMCPY

#if NUMA
    numa_free(A, input_size * sizeof(T));
    if (!p.exp) {
        numa_free(histo_host, nr_of_dpus * p.bins * sizeof(unsigned int));
    } else {
        numa_free(histo_host, p.bins * sizeof(unsigned int));
    }
#else
    free(A);
    free(histo_host);
#endif

    return 0;
}
