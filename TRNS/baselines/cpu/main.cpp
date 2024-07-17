/*
 * Copyright (c) 2016 University of Cordoba and University of Illinois
 * All rights reserved.
 *
 * Developed by:    IMPACT Research Group
 *                  University of Cordoba and University of Illinois
 *                  http://impact.crhc.illinois.edu/
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * with the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 *      > Redistributions of source code must retain the above copyright notice,
 *        this list of conditions and the following disclaimers.
 *      > Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimers in the
 *        documentation and/or other materials provided with the distribution.
 *      > Neither the names of IMPACT Research Group, University of Cordoba, 
 *        University of Illinois nor the names of its contributors may be used 
 *        to endorse or promote products derived from this Software without 
 *        specific prior written permission.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
 * THE SOFTWARE.
 *
 */

#include "support/setup.h"
#include "kernel.h"
#include "support/common.h"
#include "support/timer.h"
#include "support/verify.h"

#include <unistd.h>
#include <thread>
#include <string.h>
#include <assert.h>

#define XSTR(x) STR(x)
#define STR(x) #x

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


// Params ---------------------------------------------------------------------
struct Params {

    int n_threads;
    int n_warmup;
    int n_reps;
    int M_;
    int m;
    int N_;
    int n;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif

    Params(int argc, char **argv) {
        n_threads     = 4;
        n_warmup      = 5;
        n_reps        = 50;
        M_            = 128;
        m             = 16;
        N_            = 128;
        n             = 8;
#if NUMA
        bitmask_in    = NULL;
        bitmask_out   = NULL;
        numa_node_cpu = -1;
#endif
        int opt;
        while((opt = getopt(argc, argv, "ht:w:r:m:n:o:p:a:b:c:")) >= 0) {
            switch(opt) {
            case 'h':
                usage();
                exit(0);
                break;
            case 't': n_threads     = atoi(optarg); break;
            case 'w': n_warmup      = atoi(optarg); break;
            case 'r': n_reps        = atoi(optarg); break;
            case 'm': m             = atoi(optarg); break;
            case 'n': n             = atoi(optarg); break;
            case 'o': M_            = atoi(optarg); break;
            case 'p': N_            = atoi(optarg); break;
#if NUMA
            case 'a': bitmask_in    = numa_parse_nodestring(optarg); break;
            case 'b': bitmask_out   = numa_parse_nodestring(optarg); break;
            case 'c': numa_node_cpu = atoi(optarg); break;
#endif
            default:
                fprintf(stderr, "\nUnrecognized option!\n");
                usage();
                exit(0);
            }
        }
    }

    void usage() {
        fprintf(stderr,
                "\nUsage:  ./trns [options]"
                "\n"
                "\nGeneral options:"
                "\n    -h        help"
                "\n    -t <T>    # of host threads (default=4)"
                "\n    -w <W>    # of untimed warmup iterations (default=5)"
                "\n    -r <R>    # of timed repetition iterations (default=50)"
                "\n"
                "\nData-partitioning-specific options:"
                "\n    TRNS only supports CPU-only or GPU-only execution"
                "\n"
                "\nBenchmark-specific options:"
			        "\n    -m <I>    m (default=16 elements)"
			        "\n    -n <I>    n (default=8 elements)"
			        "\n    -o <I>    M_ (default=128 elements)"
			        "\n    -p <I>    N_ (default=128 elements)"
                "\n");
    }
};

// Input Data -----------------------------------------------------------------
void read_input(T *x_vector, const Params &p) {
    int in_size = p.M_ * p.m * p.N_ * p.n;
    srand(5432);
    for(int i = 0; i < in_size; i++) {
        x_vector[i] = ((T)(rand() % 100) / 100);
    }
}

// Main ------------------------------------------------------------------------------------------
int main(int argc, char **argv) {

    const Params p(argc, argv);
    Timer        timer;

    // Allocate
    timer.start("Allocation");
    int M_       = p.M_;
    int m       = p.m;
    int N_       = p.N_;
    int n       = p.n;
    int in_size       = M_ * m * N_ * n;
    int finished_size = M_ * m * N_;

#if NUMA
    if (p.bitmask_in) {
        numa_set_membind(p.bitmask_in);
        numa_free_nodemask(p.bitmask_in);
    }
    T *              h_in_out = (T *)numa_alloc(in_size * sizeof(T));
    std::atomic_int *h_finished =
        (std::atomic_int *)numa_alloc(sizeof(std::atomic_int) * finished_size);
#else
    T *              h_in_out = (T *)malloc(in_size * sizeof(T));
    std::atomic_int *h_finished =
        (std::atomic_int *)malloc(sizeof(std::atomic_int) * finished_size);
#endif

#if NUMA
    if (p.bitmask_out) {
        numa_set_membind(p.bitmask_out);
        numa_free_nodemask(p.bitmask_out);
    }
    std::atomic_int *h_head = (std::atomic_int *)numa_alloc(N_ * sizeof(std::atomic_int));
#else
    std::atomic_int *h_head = (std::atomic_int *)malloc(N_ * sizeof(std::atomic_int));
#endif

    ALLOC_ERR(h_in_out, h_finished, h_head);


#if NUMA
    struct bitmask *bitmask_all = numa_allocate_nodemask();
    numa_bitmask_setall(bitmask_all);
    numa_set_membind(bitmask_all);
    numa_free_nodemask(bitmask_all);
#endif

    T *h_in_backup = (T *)malloc(in_size * sizeof(T));
    ALLOC_ERR(h_in_backup);
    timer.stop("Allocation");
    //timer.print("Allocation", 1);

    // Initialize
    timer.start("Initialization");
    read_input(h_in_out, p);
    memset((void *)h_finished, 0, sizeof(std::atomic_int) * finished_size);
    for(int i = 0; i < N_; i++)
        h_head[i].store(0);
    timer.stop("Initialization");
    //timer.print("Initialization", 1);
    memcpy(h_in_backup, h_in_out, in_size * sizeof(T)); // Backup for reuse across iterations

#if NUMA
    mp_pages[0] = h_in_out;
    if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
        perror("move_pages(A)");
    }
    else if (mp_status[0] < 0) {
        printf("move_pages error: %d", mp_status[0]);
    }
    else {
        numa_node_in = mp_status[0];
    }

    mp_pages[0] = h_finished;
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



    // Loop over main kernel
    for(int rep = 0; rep < p.n_warmup + p.n_reps; rep++) {

        // Reset
        memcpy(h_in_out, h_in_backup, in_size * sizeof(T));
        memset((void *)h_finished, 0, sizeof(std::atomic_int) * finished_size);
	    for(int i = 0; i < N_; i++)
	        h_head[i].store(0);

        // start timer
        if(rep >= p.n_warmup)
            timer.start("Step 1");
        // Launch CPU threads
        std::thread main_thread_1(run_cpu_threads_100, h_in_out, h_finished, h_head, M_ * m, N_, n, p.n_threads); //M_ * m * N_);
        main_thread_1.join();
        // end timer
        if(rep >= p.n_warmup)
            timer.stop("Step 1");

        for(int i = 0; i < N_; i++)
            h_head[i].store(0);

        // start timer
        if(rep >= p.n_warmup)
            timer.start("Step 2");
        // Launch CPU threads
        std::thread main_thread_2(run_cpu_threads_010, h_in_out, h_head, m, n, M_ * N_, p.n_threads);
        main_thread_2.join();
        // end timer
        if(rep >= p.n_warmup)
            timer.stop("Step 2");

        memset((void *)h_finished, 0, sizeof(std::atomic_int) * finished_size);
        for(int i = 0; i < N_; i++)
            h_head[i].store(0);

        // start timer
        if(rep >= p.n_warmup)
            timer.start("Step 3");
        // Launch CPU threads
        for(int i = 0; i < N_; i++){
            std::thread main_thread_3(run_cpu_threads_100, h_in_out + i * M_ * n * m, h_finished + i * M_ * n, h_head + i, M_, n, m, p.n_threads); //M_ * n);
            main_thread_3.join();
        }
        // end timer
        if(rep >= p.n_warmup)
            timer.stop("Step 3");

        if (rep >= p.n_warmup) {
            printf("[::] TRNS-CPU | n_threads=%d e_type=%s n_elements=%d"
#if NUMA
                " numa_node_in=%d numa_node_out=%d numa_node_cpu=%d numa_distance_in_cpu=%d numa_distance_cpu_out=%d"
#endif
                " | throughput_MBps=%f",
                p.n_threads, XSTR(T), in_size,
#if NUMA
                numa_node_in, numa_node_out, numa_node_cpu, numa_distance(numa_node_in, numa_node_cpu), numa_distance(numa_node_cpu, numa_node_out),
#endif
                in_size * sizeof(T) / (timer.get("Step 1") + timer.get("Step 2") + timer.get("Step 3")));
            printf(" throughput_MOpps=%f",
                in_size / (timer.get("Step 1") + timer.get("Step 2") + timer.get("Step 3")));
            printf(" timer1_us=%f timer2_us=%f timer3_us=%f\n",
                timer.get("Step 1"), timer.get("Step 2"), timer.get("Step 3"));
        }
    }
    //timer.print("Step 1", p.n_reps);
    //timer.print("Step 2", p.n_reps);
    //timer.print("Step 3", p.n_reps);

    // Verify answer
    //verify(h_in_out, h_in_backup, M_ * m, N_ * n, 1);

    // Free memory
    timer.start("Deallocation");
#if NUMA
    numa_free(h_in_out, in_size * sizeof(T));
    numa_free(h_finished, sizeof(std::atomic_int) * finished_size);
    numa_free(h_head, N_ * sizeof(std::atomic_int));
    numa_free(h_in_backup, in_size * sizeof(T));
#else
    free(h_in_out);
    free(h_finished);
    free(h_head);
    free(h_in_backup);
#endif
    timer.stop("Deallocation");
    //timer.print("Deallocation", 1);

    printf("Test Passed\n");
    return 0;
}
