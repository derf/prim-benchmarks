
#ifndef _PARAMS_H_
#define _PARAMS_H_

#include "common.h"
#include "utils.h"

static void usage()
{
	PRINT("\nUsage:  ./program [options]"
	      "\n"
	      "\nBenchmark-specific options:"
	      "\n    -f <F>    input matrix file name (default=data/roadNet-CA.txt)"
	      "\n"
	      "\nGeneral options:"
	      "\n    -v <V>    verbosity" "\n    -h        help" "\n\n");
}

typedef struct Params {
	const char *fileName;
	unsigned int verbosity;
#if NUMA
	struct bitmask *bitmask_in;
	int numa_node_cpu;
#endif
} Params;

static struct Params input_params(int argc, char **argv)
{
	struct Params p;
	p.fileName = "data/roadNet-CA.txt";
	p.verbosity = 0;
#if NUMA
	p.bitmask_in = NULL;
	p.numa_node_cpu = -1;
#endif
	int opt;
	while ((opt = getopt(argc, argv, "f:v:hA:C:")) >= 0) {
		switch (opt) {
		case 'f':
			p.fileName = optarg;
			break;
		case 'v':
			p.verbosity = atoi(optarg);
			break;
#if NUMA
		case 'A':
			p.bitmask_in = numa_parse_nodestring(optarg);
			break;
		case 'C':
			p.numa_node_cpu = atoi(optarg);
			break;
#endif
		case 'h':
			usage();
			exit(0);
		default:
			PRINT_ERROR("Unrecognized option!");
			usage();
			exit(0);
		}
	}

	return p;
}

#endif
