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

perf_attr_t perf_attrs[] = {
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, "n_instr" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES, "n_cache_ref" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, "n_cache_miss" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS, "n_branch" },
	{ PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES, "n_branch_miss" },
	/*
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_FRONTEND, "n_stalled_frontend"},
	{PERF_TYPE_HARDWARE, PERF_COUNT_HW_STALLED_CYCLES_BACKEND,  "n_stalled_backend"},
	*/
	{
	    PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_L1D
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "n_l1d_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_L1D
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "n_l1d_read_miss" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_LL
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "n_llc_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_LL
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "n_llc_read_miss" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_DTLB
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16),
	    "n_tlb_read" },
	{ PERF_TYPE_HW_CACHE,
	    PERF_COUNT_HW_CACHE_DTLB
	        | (PERF_COUNT_HW_CACHE_OP_READ << 8)
	        | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16),
	    "n_tlb_read_miss" },
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
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_measurements[i] = perf_alloc(perf_attrs[i].type, perf_attrs[i].config, i);
	}
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		perf_open_measurement(perf_measurements[i], -1, 0);
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
	for (unsigned long int i = 0; i < sizeof(perf_attrs) / sizeof(perf_attr_t); i++) {
		printf(" %s=%lu", perf_attrs[i].desc, measurements[i].values[0].value);
	}
	printf("\n");
}
