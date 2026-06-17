#pragma once

#include <linux/perf_event.h>
#include <perf/utilities.h>

typedef struct {
	uint64_t nr;
	struct {
		uint64_t value;
		uint64_t id;
	} values[1];
} measurement_t;

typedef struct {
	int type;
	int config;
	char* desc;
} perf_attr_t;

/*
 * Metrics used by bailey2014icpp:
 * - L2D misses
 * - L1D accesses
 * - TLB misses
 * - conditional branch instructions
 * - vector instructions
 * - stalled core cycles
 * - total core cycles
 * - reference cycles
 * - idle FPU cycles
 * - interrupts
 * All of them normalized to #instr / #refcycles
 */

/*
 * PERF_COUNT_HW_INSTRUCTIONS is used for normalization and must be the first
 * item in the array.
 */
perf_attr_t perf_attrs[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "instr" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "cache_ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "cache_miss" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "branch" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, "branch_miss" },
	/*
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, "stalled_frontend"},
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND,  "stalled_backend"},
	*/
	{
	    PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_L1D
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "l1d_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_L1D
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "l1d_read_miss" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_LL
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "llc_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_LL
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "llc_read_miss" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_DTLB
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "tlb_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_DTLB
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "tlb_read_miss" },
};

perf_measurement_t* perf_measurements[sizeof(perf_attrs) / sizeof(perf_attr_t)];
measurement_t measurements[sizeof(perf_attrs) / sizeof(perf_attr_t)];

perf_measurement_t* perf_alloc(int type, int config, unsigned long int attr_idx)
{
	perf_measurement_t* ret = perf_create_measurement(type, config, 0, -1);
	if (perf_has_sufficient_privilege(ret) != 1) {
		printf("cannot measure %s: insufficient privilege\n", perf_attrs[attr_idx].desc);
		exit(1);
	}
	if (perf_event_is_supported(ret) != 1) {
		printf("cannot measure %s: unsupported\n", perf_attrs[attr_idx].desc);
		exit(1);
	}
	return ret;
}

void perf_start()
{
	int group = -1;
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_measurements[i] = perf_alloc(perf_attrs[i].type, perf_attrs[i].config, i);
	}
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_open_measurement(perf_measurements[i], group, 0);
		if (i == 0) {
			group = perf_measurements[0]->group;
		}
	}
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_start_measurement(perf_measurements[i]);
	}
}

void perf_stop()
{
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_stop_measurement(perf_measurements[i]);
	}
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_read_measurement(perf_measurements[i], &measurements[i], sizeof(measurement_t));
		perf_close_measurement(perf_measurements[i]);
		free((void*)perf_measurements[i]);
	}
}

void perf_print()
{
	/*
	 * By convention, perf_attrs[0] is #instr
	 */
	printf(" n_%s=%lu", perf_attrs[0].desc, measurements[0].values[0].value);
	for (unsigned long int i = 1; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
#if PERF_PRINT_RAW_VALUES
		printf(" n_%s=%lu", perf_attrs[i].desc, measurements[i].values[0].value);
#else
		/*
		 * normalize to events per instruction
		 */
		printf(" ratio_%s=%f", perf_attrs[i].desc, (double)measurements[i].values[0].value / (double)measurements[0].values[0].value);
#endif
	}
	printf("\n");
}
