#include <stdlib.h>
#include <stdio.h>
#include "../../support/timer.h"

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

#include "gemv_utils.h"

int main(int argc, char *argv[])
{
    (void) argc;
    const size_t rows = 20480;
    const size_t cols = 8192;

    double **A, *b, *x;

#if NUMA
    bitmask_in    = numa_parse_nodestring(argv[1]);
    bitmask_out   = numa_parse_nodestring(argv[2]);
    numa_node_cpu = atoi(argv[3]);
#endif

#if NUMA
    if (bitmask_out) {
        numa_set_membind(bitmask_out);
        numa_free_nodemask(bitmask_out);
    }
    b = (double*) numa_alloc(sizeof(double)*rows);
#else
    b = (double*) malloc(sizeof(double)*rows);
#endif

#if NUMA
    if (bitmask_in) {
        numa_set_membind(bitmask_in);
        // no free yet, re-used in allocate_dense
    }
    x = (double*) numa_alloc(sizeof(double)*cols);
#else
    x = (double*) malloc(sizeof(double)*cols);
#endif

    allocate_dense(rows, cols, &A);

    make_hilbert_mat(rows,cols, &A);

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
        printf("move_pages(A) error: %d", mp_status[0]);
    }
    else {
        numa_node_in = mp_status[0];
    }

    mp_pages[0] = b;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(b)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages(b) error: %d", mp_status[0]);
    }
    else {
        numa_node_out = mp_status[0];
    }

    if (numa_node_cpu != -1) {
        if (numa_run_on_node(numa_node_cpu) == -1) {
            perror("numa_run_on_node");
            numa_node_cpu = -1;
        }
    }
#endif

    Timer timer;
    for (int i = 0; i < 100; i++) {

#pragma omp parallel
        {
#pragma omp for
        for (size_t i = 0; i < cols; i++) {
          x[i] = (double) i+1 ;
        }

#pragma omp for
        for (size_t i = 0; i < rows; i++) {
          b[i] = (double) 0.0;
        }
        }

        unsigned int nr_threads = 0;
#pragma omp parallel
#pragma omp atomic
        nr_threads++;

        start(&timer, 0, 0);
        gemv(A, x, rows, cols, &b);
        stop(&timer, 0);
        printf("[::] GEMV CPU | n_threads=%d e_type=%s n_elements=%ld"
#if NUMA
            " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
            " | throughput_MBps=%f",
            nr_threads, "double", rows * cols,
#if NUMA
            numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
            rows * cols * sizeof(double) / timer.time[0]);
        printf(" throughput_MOpps=%f",
            rows * cols / timer.time[0]);
        printall(&timer, 0);
    }


#if 0
  print_vec(x, rows);
  print_mat(A, rows, cols);
  print_vec(b, rows);
#endif

  printf("sum(x) = %f, sum(Ax) = %f\n", sum_vec(x,cols), sum_vec(b,rows));

#if NUMA
  numa_free(b, sizeof(double)*rows);
  numa_free(x, sizeof(double)*cols);
  numa_free(*A, sizeof(double)*rows*cols);
  numa_free(A, sizeof(double)*rows);
#else
  free(b);
  free(x);
  free(*A);
  free(A);
#endif

  return 0;
}

void gemv(double** A, double* x, size_t rows, size_t cols, double** b) {
#pragma omp parallel for
  for (size_t i = 0; i < rows; i ++ )
  for (size_t j = 0; j < cols; j ++ ) {
    (*b)[i] = (*b)[i] + A[i][j]*x[j];
  }
}

void make_hilbert_mat(size_t rows, size_t cols, double*** A) {
#pragma omp parallel for
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      (*A)[i][j] = 1.0/( (double) i + (double) j + 1.0);
    }
  }
}

double sum_vec(double* vec, size_t rows) {
  double sum = 0.0;
#pragma omp parallel for reduction(+:sum)
  for (int i = 0; i < rows; i++) sum = sum + vec[i];
  return sum;
}
