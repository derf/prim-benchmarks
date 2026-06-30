#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <omp.h>

#ifndef T
#define T double
#endif

#if DFATOOL_TIMING
#include "../../include/timer.h"
#else
#define start(...)
#define stop(...)
#endif

#if WITH_PERF_LIB
#include "../../../include/perf-lib.h"
#elif WITH_PERF_EXT
#include "../../../include/perf-ext.h"
#else
#define perf_start(...)
#define perf_stop(...)
#endif

/*
 * NUMA_MEMCPY is not supported at the moment.
 */

#if NUMA
#include <numa.h>
#include <numaif.h>

struct bitmask* bitmask_in;
struct bitmask* bitmask_out;

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;
#endif

#include "../../include/params.h"

#define XSTR(x) STR(x)
#define STR(x) #x

#include "gemv_utils.h"

int main(int argc, char* argv[])
{
	struct Params p = input_params(argc, argv);

	const size_t rows = p.m_size;
	const size_t cols = p.n_size;

	omp_set_num_threads(p.n_threads);

	T **A, *b, *x;

	T **A_local, *x_local;

#if NUMA
	if (p.bitmask_out != NULL) {
		numa_set_membind(p.bitmask_out);
		numa_free_nodemask(p.bitmask_out);
	}
	b = (T*)numa_alloc(sizeof(T) * rows);
#else
	b = (T*)malloc(sizeof(T) * rows);
#endif

#if NUMA
	if (p.bitmask_in != NULL) {
		numa_set_membind(p.bitmask_in);
		// no free yet, re-used in allocate_dense
	}
	x = (T*)numa_alloc(sizeof(T) * cols);
#else
	x = (T*)malloc(sizeof(T) * cols);
#endif

	allocate_dense(rows, cols, &A);

#if NUMA
	if (p.bitmask_in != NULL) {
		numa_free_nodemask(p.bitmask_in);
	}
#endif

	make_hilbert_mat(rows, cols, &A);

#if NUMA
	struct bitmask* bitmask_all = numa_allocate_nodemask();
	numa_bitmask_setall(bitmask_all);
	numa_set_membind(bitmask_all);
	numa_free_nodemask(bitmask_all);
#endif // NUMA

	A_local = A;
	x_local = x;

#if NUMA
	if (p.bitmask_in) {
		mp_pages[0] = A;
		if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
			perror("move_pages(A)");
		} else if (mp_status[0] < 0) {
			printf("move_pages(A) error: %d", mp_status[0]);
		} else {
			numa_node_in = mp_status[0];
		}
	}

	if (p.bitmask_out) {
		mp_pages[0] = b;
		if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
			perror("move_pages(b)");
		} else if (mp_status[0] < 0) {
			printf("move_pages(b) error: %d", mp_status[0]);
		} else {
			numa_node_out = mp_status[0];
		}
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
	numa_node_in_is_local = ((numa_node_cpu == numa_node_in)
	                            || (numa_node_cpu + 8 == numa_node_in))
	    * 1;
#endif

#if DFATOOL_TIMING
	Timer timer;
#endif

	for (unsigned int i = 0; i < p.n_warmup + p.n_reps; i++) {

#pragma omp parallel
		{
#pragma omp for
			for (size_t i = 0; i < cols; i++) {
				x[i] = (T)i + 1;
			}

#pragma omp for
			for (size_t i = 0; i < rows; i++) {
				b[i] = (T)0;
			}
		}

#if NUMA_MEMCPY
		if (i >= p.n_warmup) {
			start(&timer, 1, 0);
		}
		if (!numa_node_in_is_local) {
			x_local = (T*)numa_alloc(sizeof(T) * cols);
			allocate_dense(rows, cols, &A_local);
		}
		if (i >= p.n_warmup) {
			stop(&timer, 1);
		}

		if (x_local == NULL) {
			return 1;
		}
		if (A_local == NULL) {
			return 1;
		}

		if (!numa_node_in_is_local) {
			if (numa_node_cpu_memcpy != -1) {
				if (numa_run_on_node(numa_node_cpu_memcpy) == -1) {
					perror("numa_run_on_node");
					numa_node_cpu_memcpy = -1;
				}
			}
		}

		if (i >= p.n_warmup) {
			start(&timer, 2, 0);
		}
		if (!numa_node_in_is_local) {
			// for (size_t i=0; i < rows; i++ ) {
			//    memcpy(A_local[i], A[i], cols * sizeof(T));
			// }
			memcpy(*A_local, *A, rows * cols * sizeof(T));
			memcpy(x_local, x, cols * sizeof(T));
		} else {
			A_local = A;
			x_local = x;
		}
		if (i >= p.n_warmup) {
			stop(&timer, 2);
		}

		if (numa_node_cpu != -1) {
			if (numa_run_on_node(numa_node_cpu) == -1) {
				perror("numa_run_on_node");
				numa_node_cpu = -1;
			}
		}

		mp_pages[0] = A_local;
		if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
			perror("move_pages(A_local)");
		} else if (mp_status[0] < 0) {
			printf("move_pages error: %d", mp_status[0]);
		} else {
			numa_node_local = mp_status[0];
		}
#endif

		unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
		nr_threads++;

		if (i >= p.n_warmup) {
			perf_start();
			start(&timer, 0, 0);
		}
		gemv(A_local, x_local, rows, cols, &b);
		if (i >= p.n_warmup) {
			stop(&timer, 0);
			perf_stop();
		}

#if NUMA_MEMCPY
			perf_start();
			start(&timer, 3, 0);
		}
		if (!numa_node_in_is_local) {
			numa_free(x_local, sizeof(T) * cols);
			numa_free(*A_local, sizeof(T) * rows * cols);
			numa_free(A_local, sizeof(void*) * rows);
		}
		if (i >= p.n_warmup) {
			stop(&timer, 3);
		}
#endif

		if (i >= p.n_warmup) {
#if NUMA_MEMCPY
			printf("[::] GEMV-CPU-MEMCPY | n_threads=%d e_type=%s n_elements=%ld"
				" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_node_local=%d numa_node_cpu_memcpy=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
				" | throughput_MBps=%f throughput_MOpps=%f",
				nr_threads,
				XSTR(T), rows * cols, numa_node_in, numa_node_out,
				numa_node_cpu, numa_node_local, numa_node_cpu_memcpy,
				numa_distance(numa_node_in, numa_node_cpu),
				numa_distance(numa_node_cpu, numa_node_out),
				rows * cols * sizeof(T) / timer.time[0],
				rows * cols / timer.time[0]);
			printf(" latency_kernel_us=%f latency_alloc_us=%f latency_memcpy_us=%f latency_free_us=%f latency_total_us=%f\n",
				timer.time[0], timer.time[1], timer.time[2], timer.time[3],
				timer.time[0] + timer.time[1] + timer.time[2] + timer.time[3]);
#else // !NUMA_MEMCPY
#if WITH_PERF_LIB
			printf("[::] GEMV-CPU | n_threads=%d e_type=%s n_elements=%ld"
#if NUMA
				" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
				" |",
				nr_threads, XSTR(T), rows * cols
#if NUMA
			    ,
			    numa_node_in, numa_node_out, numa_node_cpu,
			    numa_distance(numa_node_in, numa_node_cpu),
			    numa_distance(numa_node_cpu, numa_node_out)
#endif
			);
			perf_print();
#elif DFATOOL_TIMING
			printf("[::] GEMV-CPU | n_threads=%d e_type=%s n_elements=%ld"
#if NUMA
				" numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
				" | throughput_MBps=%f",
				nr_threads, XSTR(T), rows * cols,
#if NUMA
				numa_node_in, numa_node_out, numa_node_cpu,
				numa_distance(numa_node_in, numa_node_cpu),
				numa_distance(numa_node_cpu, numa_node_out),
#endif
				rows * cols * sizeof(T) / timer.time[0]);
			printf(" throughput_MOpps=%f latency_us=%f\n",
				rows * cols / timer.time[0], timer.time[0]);
#endif // DFATOOL_TIMING
#endif // !NUMA_MEMCPY
		}
	}

#if 0
	print_vec(x, rows);
	print_mat(A, rows, cols);
	print_vec(b, rows);
#endif

#if TYPE_double || TYPE_float
	printf("sum(x) = %f, sum(Ax) = %f\n", sum_vec(x, cols),
	    sum_vec(b, rows));
#else
	printf("sum(x) = %d, sum(Ax) = %d\n", sum_vec(x, cols),
	    sum_vec(b, rows));
#endif

#if NUMA
	numa_free(b, sizeof(T) * rows);
	numa_free(x, sizeof(T) * cols);
	numa_free(*A, sizeof(T) * rows * cols);
	numa_free(A, sizeof(void*) * rows);
#else
	free(b);
	free(x);
	free(*A);
	free(A);
#endif

	return 0;
}

void gemv(T** A, T* x, size_t rows, size_t cols, T** b)
{
#pragma omp parallel for
	for (size_t i = 0; i < rows; i++)
		for (size_t j = 0; j < cols; j++) {
			(*b)[i] = (*b)[i] + A[i][j] * x[j];
		}
}

void make_hilbert_mat(size_t rows, size_t cols, T*** A)
{
#pragma omp parallel for
	for (size_t i = 0; i < rows; i++) {
		for (size_t j = 0; j < cols; j++) {
#if TYPE_double || TYPE_float
			(*A)[i][j] = 1.0 / ((T)i + (T)j + 1.0);
#else
			(*A)[i][j] = (T)(((i + j) % 10));
#endif
		}
	}
}

T sum_vec(T* vec, size_t rows)
{
	T sum = 0;
#pragma omp parallel for reduction(+ : sum)
	for (int i = 0; i < rows; i++)
		sum = sum + vec[i];
	return sum;
}
