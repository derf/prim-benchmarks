#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <assert.h>
#include <time.h>
#include <stdint.h>
#include "timer.h"

#if NUMA
#include <numaif.h>
#include <numa.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
struct bitmask* bitmask_in;
int numa_node_in = -1;
int numa_node_cpu = -1;
#endif

#define DTYPE uint64_t
/*
* @brief creates a "test file" by filling a bufferwith values
*/
void create_test_file(DTYPE * input, uint64_t  nr_elements, DTYPE * querys, uint64_t n_querys) {

  srand(time(NULL));

  input[0] = 1;
  for (uint64_t i = 1; i < nr_elements; i++) {
        input[i] = input[i - 1] + (rand() % 10) + 1;
  }

  for(uint64_t i = 0; i < n_querys; i++)
  {
	querys[i] = input[rand() % (nr_elements - 2)];
  }
}

/**
* @brief compute output in the host
*/
uint64_t binarySearch(DTYPE * input, uint64_t input_size, DTYPE* querys, unsigned n_querys)
{

	uint64_t found = -1;
	uint64_t q, r, l, m;
	
       #pragma omp parallel for private(q,r,l,m)
     	for(q = 0; q < n_querys; q++)
      	{
		l = 0;
		r = input_size;
		while (l <= r) 
		{
	    		m = l + (r - l) / 2;

	    		// Check if x is present at mid
	     		if (input[m] == querys[q])
			{	
		    		found += m;
				break;
			}
	    		// If x greater, ignore left half
	    		if (input[m] < querys[q])
			    	l = m + 1;

	    		// If x is smaller, ignore right half
			else
		    		r = m - 1;
		
		}
       	}

      	return found;
}

  /**
  * @brief Main of the Host Application.
  */
  int main(int argc, char **argv) {
    (void)argc;
    Timer timer;
    uint64_t input_size = atol(argv[1]);
    uint64_t n_querys = atol(argv[2]);
#if NUMA
    bitmask_in = numa_parse_nodestring(argv[3]);
    numa_node_cpu = atoi(argv[4]);
#endif

    printf("Vector size: %lu, num searches: %lu\n", input_size, n_querys);

#if NUMA
    if (bitmask_in) {
        numa_set_membind(bitmask_in);
        numa_free_nodemask(bitmask_in);
    }
    DTYPE * input = numa_alloc((input_size) * sizeof(DTYPE));
    DTYPE * querys = numa_alloc((n_querys) * sizeof(DTYPE));
#else
    DTYPE * input = malloc((input_size) * sizeof(DTYPE));
    DTYPE * querys = malloc((n_querys) * sizeof(DTYPE));
#endif

#if NUMA
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif

    DTYPE result_host = -1;

    // Create an input file with arbitrary data.
    create_test_file(input, input_size, querys, n_querys);

#if NUMA
    mp_pages[0] = input;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_in = mp_status[0];
    }

    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#endif

    start(&timer, 0, 0);
    result_host = binarySearch(input, input_size - 1, querys, n_querys);
    stop(&timer, 0);

    unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
    nr_threads++;

    int status = (result_host);
    if (status) {
        printf("[::] BS-CPU | n_threads=%d e_type=%s n_elements=%lu"
#if NUMA
            " numa_node_in=%d numa_node_cpu=%d numa_distance_in_cpu=%d"
#endif
            " | throughput_MBps=%f",
            nr_threads, "uint64_t", input_size,
#if NUMA
            numa_node_in, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu),
#endif
            n_querys * sizeof(DTYPE) / timer.time[0]);
        printf(" throughput_MOpps=%f",
            n_querys / timer.time[0]);
        printall(&timer, 0);
    } else {
        printf("[ERROR]\n");
    }

#if NUMA
    numa_free(input, input_size * sizeof(DTYPE));
    numa_free(querys, n_querys * sizeof(DTYPE));
#else
    free(input);
    free(querys);
#endif


    return status ? 0 : 1;
}

