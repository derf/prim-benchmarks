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
#include "../../support/common.h"

#if WITH_BENCHMARK
#include "../../support/timer.h"
#else
#define start(...)
#define stop(...)
#endif

#if NUMA
#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_data = -1;
int numa_node_cpu = -1;
#endif

#define XSTR(x) STR(x)
#define STR(x) #x

// weights
T** A;

// input/output
T* B;

// intermediate
T* C;

// Create input arrays
static void init_data(T** A, unsigned int m_size, unsigned int n_size){
    for (unsigned int l = 0; l < NUM_LAYERS; l++) {
		for (unsigned int i = 0; i < m_size * n_size; i++){
			if(i % 100 < 98){
				A[l][i] = 0;
			}else{
				A[l][i] = (l+i) % 2;
			}
		}
	}
}

static void init_B(T* B, unsigned int n_size){
	for (unsigned int i = 0; i < n_size; i++){
		if(i % 50 < 48){
			B[i] = 0;
		}
		else{
			B[i] = i % 2;
		}
	}
}

// Compute output in the host
static void mlp_host(T* C, T** A, T* B, unsigned int m_size, unsigned int n_size) {
	for (unsigned int nl = 0; nl < NUM_LAYERS; nl++){
		for (unsigned int m = 0; m < m_size; m++){
			C[m] = 0;
		}
		#pragma omp parallel for
		for (unsigned int m = 0; m < m_size; m++){
			for (unsigned int n = 0; n < n_size; n++){
				C[m] += A[nl][m * n_size + n] * B[n];
			}
			C[m] = max(0, C[m]);
		}
		for (unsigned int n = 0; n < n_size; n++){
			B[n] = C[n];
		}
	}
}

static uint64_t mlp_host_sum(uint64_t n_size) {
  uint64_t sum = 0;
  for (uint64_t m = 0; m < n_size; m++){
    sum += B[m];
  }
  return sum;
}

// Params ---------------------------------------------------------------------
typedef struct Params {
  int   input_size_n;
  int   input_size_m;
  int   n_reps;
#if NUMA
  struct bitmask* bitmask;
  int numa_node_cpu;
#endif
}Params;

void usage() {
  fprintf(stderr,
    "\nUsage:  ./program [options]"
    "\n");
}

  struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size_n  = 8192;
    p.input_size_m  = 20480;
    p.n_reps        = 100;
#if NUMA
    p.bitmask = NULL;
    p.numa_node_cpu = -1;
#endif

    int opt;
    while((opt = getopt(argc, argv, "e:n:m:A:C:")) >= 0) {
      switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'e': p.n_reps = atoi(optarg); break;
        case 'n': p.input_size_n    = atoi(optarg); break;
        case 'm': p.input_size_m    = atoi(optarg); break;
#if NUMA
        case 'A': p.bitmask         = numa_parse_nodestring(optarg); break;
        case 'C': p.numa_node_cpu  = atoi(optarg); break;
#endif
        default:
        fprintf(stderr, "\nUnrecognized option!\n");
        usage();
        exit(0);
      }
    }

    return p;
  }

  /**
  * @brief Main of the Host Application.
  */
  int main(int argc, char **argv) {

    struct Params p = input_params(argc, argv);
    uint64_t n_size = p.input_size_n;
    uint64_t m_size = p.input_size_m;

#if WITH_BENCHMARK
    Timer timer;
#endif

#if NUMA
    if (p.bitmask) {
        numa_set_membind(p.bitmask);
        numa_free_nodemask(p.bitmask);
    }
    A = numa_alloc(NUM_LAYERS * sizeof(T*));
    for(int l = 0; l < NUM_LAYERS; l++) {
        A[l] = numa_alloc(n_size*m_size*sizeof(unsigned int));
    }
    B = numa_alloc(m_size*sizeof(unsigned int));
    C = numa_alloc(m_size*sizeof(unsigned int));

    mp_pages[0] = A;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_data = mp_status[0];
    }

    numa_node_cpu = p.numa_node_cpu;
    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#else
    A = malloc(NUM_LAYERS * sizeof(T*));
    for(int l = 0; l < NUM_LAYERS; l++) {
        A[l] = malloc(n_size*m_size*sizeof(unsigned int));
    }
    B = malloc(m_size*sizeof(unsigned int));
    C = malloc(m_size*sizeof(unsigned int));
#endif

    // Create an input file with arbitrary data.
    init_data(A, m_size, n_size);

    for (int i = 0; i < p.n_reps; i++) {
        init_B(B, n_size);

        start(&timer, 0, 0);
        mlp_host(C, A, B, n_size, m_size);
        stop(&timer, 0);

#if WITH_BENCHMARK
        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        printf("[::] MLP-CPU | n_threads=%d e_type=%s n_elements=%lu",
            nr_threads, XSTR(T), n_size * m_size);
#if NUMA
        printf(" numa_node_data=%d numa_node_cpu=%d numa_distance_cpu_data=%d",
            numa_node_data, numa_node_cpu, numa_distance(numa_node_data, numa_node_cpu));
#endif
        printf(" | throughput_MBps=%f throughput_MOpps=%f",
            n_size * m_size * sizeof(T) / timer.time[0],
            n_size * m_size / timer.time[0]);
        printf(" latency_us=%f\n",
            timer.time[0]);
#endif // WITH_BENCHMARK
    }

#if NOP_SYNC
    for(int rep = 0; rep < 200000; rep++) {
        asm volatile("nop" ::);
    }
#endif

    uint32_t sum = mlp_host_sum(n_size);

    printf("SUM = %d \n", sum);

#if NUMA
    for(int l = 0; l < NUM_LAYERS; l++) {
        numa_free(A[l], n_size*m_size*sizeof(unsigned int));
    }
    numa_free(A, NUM_LAYERS * sizeof(T*));
    numa_free(B, m_size*sizeof(unsigned int));
    numa_free(C, m_size*sizeof(unsigned int));
#else
    for(int l = 0; l < NUM_LAYERS; l++) {
        free(A[l]);
    }
    free(A);
    free(B);
    free(C);
#endif

    return 0;
}
