void allocate_dense(size_t rows,size_t  cols, double*** dense) {

#if NUMA
    if (bitmask_in) {
        numa_set_membind(bitmask_in);
        numa_free_nodemask(bitmask_in);
    }
  *dense = numa_alloc(sizeof(double)*rows);
  **dense = numa_alloc(sizeof(double)*rows*cols);
#else
  *dense = malloc(sizeof(double)*rows);
  **dense = malloc(sizeof(double)*rows*cols);
#endif

  for (size_t i=0; i < rows; i++ ) {
    (*dense)[i] = (*dense)[0] + i*cols;
  }

}

void print_mat(double** A, size_t rows, size_t cols) {
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      printf("%f ", A[i][j]);
    }
    printf("\n");
  }
}

void print_vec(double* b, size_t rows) {
  for (size_t i = 0; i < rows; i++) {
    printf("%f\n", b[i]);
  }
}

void gemv(double** A, double* x, size_t rows, size_t cols, double** b);
void make_hilbert_mat(size_t rows, size_t cols, double*** A);
double sum_vec(double* vec, size_t rows);
