#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "common.h"

#if NUMA
#include <numaif.h>
#include <numa.h>
#endif

typedef struct Params {
    unsigned int   input_size;
    unsigned int   n_threads;
    unsigned int   n_nops;
    unsigned int   n_instr;
    int   n_warmup;
    int   n_reps;
    int  exp;
#if NUMA
    struct bitmask* bitmask_in;
    struct bitmask* bitmask_out;
    int numa_node_cpu;
#endif
}Params;

static void usage() {
    fprintf(stderr,
        "\nUsage:  ./program [options]"
        "\n"
        "\nGeneral options:"
        "\n    -h        help"
        "\n    -w <W>    # of untimed warmup iterations (default=1)"
        "\n    -e <E>    # of timed repetition iterations (default=3)"
        "\n    -x <X>    Weak (0) or strong (1) scaling (default=0)"
        "\n"
        "\nBenchmark-specific options:"
        "\n    -i <I>    input size (default=8K elements)"
        "\n    -N <N>    number of nops in dpu task (default=0)"
        "\n    -I <N>    number of instructions in dpu binary (default=0)"
        "\n    -a <spec> allocate input data on specified NUMA node(s)"
        "\n    -b <spec> allocate output data on specified NUMA node(s)"
        "\n    -c <spec> run on specified NUMA node(s) -- does not affect transfer thread pool"
        "\n");
}

struct Params input_params(int argc, char **argv) {
    struct Params p;
    p.input_size    = 8 << 10;
    p.n_threads     = 8;
    p.n_nops        = 0;
    p.n_instr       = 0;
    p.n_warmup      = 1;
    p.n_reps        = 3;
    p.exp           = 0;
#if NUMA
    p.bitmask_in     = NULL;
    p.bitmask_out    = NULL;
    p.numa_node_cpu = -1;
#endif

    int opt;
    while((opt = getopt(argc, argv, "hi:n:w:e:x:N:I:a:b:c:")) >= 0) {
        switch(opt) {
        case 'h':
        usage();
        exit(0);
        break;
        case 'i': p.input_size    = atoi(optarg); break;
        case 'n': p.n_threads     = atoi(optarg); break;
        case 'N': p.n_nops        = atoi(optarg); break;
        case 'I': p.n_instr       = atoi(optarg); break;
        case 'w': p.n_warmup      = atoi(optarg); break;
        case 'e': p.n_reps        = atoi(optarg); break;
        case 'x': p.exp           = atoi(optarg); break;
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

    return p;
}
#endif
