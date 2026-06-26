
#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "common.h"
#include "utils.h"

static void usage()
{
	PRINT("\nUsage:  ./program [options]"
	      "\n"
	      "\nBenchmark-specific options:"
	      "\n    -f <F>    input matrix file name (default=data/bcsstk30.mtx)"
	      "\n"
	      "\nGeneral options:"
	      "\n    -w <W>    # of untimed warmup iterations (default=1)"
	      "\n    -e <E>    # of timed repetition iterations (default=3)"
	      "\n    -v <V>    verbosity"
	      "\n    -h        help"
	      "\n\n");
}

typedef struct Params {
	const char* fileName;
	int n_warmup;
	int n_reps;
	int n_threads;
	unsigned int verbosity;
#if NUMA
	struct bitmask* bitmask_in;
	struct bitmask* bitmask_out;
	int numa_node_cpu;
#endif
} Params;

static struct Params input_params(int argc, char** argv)
{
	struct Params p;
	p.fileName = "data/bcsstk30.mtx";
	p.n_warmup = 1;
	p.n_reps = 3;
	p.n_threads = 4;
	p.verbosity = 1;
#if NUMA
	p.bitmask_in = NULL;
	p.bitmask_out = NULL;
	p.numa_node_cpu = -1;
#endif
	int opt;
	while ((opt = getopt(argc, argv, "f:v:w:e:t:hA:B:C:")) >= 0) {
		switch (opt) {
		case 'f':
			p.fileName = optarg;
			break;
		case 'w':
			p.n_warmup = atoi(optarg);
			break;
		case 'e':
			p.n_reps = atoi(optarg);
			break;
		case 't':
			p.n_threads = atoi(optarg);
			break;
		case 'v':
			p.verbosity = atoi(optarg);
			break;
		case 'h':
			usage();
			exit(0);
#if NUMA
		case 'A':
			p.bitmask_in = numa_parse_nodestring(optarg);
			break;
		case 'B':
			p.bitmask_out = numa_parse_nodestring(optarg);
			break;
		case 'C':
			p.numa_node_cpu = atoi(optarg);
			break;
#endif // NUMA
		default:
			PRINT_ERROR("Unrecognized option!");
			usage();
			exit(0);
		}
	}

	return p;
}

#endif
