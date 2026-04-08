/*
 * Copyright 2026 Birte Kristina Friesel <birte.friesel@uni-osnabrueck.de>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <alloc.h>
#include <handshake.h>
#include <string.h>

#include "common.h"

__host dpu_arguments_t DPU_INPUT_ARGUMENTS;

uint32_t tasklet_count[NR_TASKLETS];

extern int main_kernel_select(void);

int (*kernels[nr_kernels])(void) = {main_kernel_select};

int main(void) {
	return kernels[DPU_INPUT_ARGUMENTS.kernel]();
}

unsigned int _pred_f(const T x)
{
	return ((x % 2) == 0) * 1;
}

int main_kernel_select()
{
	unsigned int tasklet_id = me();

	if (tasklet_id == 0) {
		mem_reset();
	}

	uint32_t input_size_dpu_bytes = DPU_INPUT_ARGUMENTS.size;

	// Address of the current processing block in MRAM
	uint32_t base_tasklet = tasklet_id << BLOCK_SIZE_LOG2;
	uint32_t mram_base_addr = (uint32_t)DPU_MRAM_HEAP_POINTER;
	uint32_t bitmask_base_addr = mram_base_addr + 65011712;

	T* cache = (T*) mem_alloc(BLOCK_SIZE);
	uint32_t* bitmask_cache = (uint32_t*) mem_alloc(BLOCK_SIZE / sizeof(T) / 32 * sizeof(uint32_t));

	// bool (*_pred_f)(uint64_t const, uint64_t const) = get_pred(DPU_INPUT_ARGUMENTS.predicate);

	for (unsigned int byte_index = base_tasklet; byte_index < input_size_dpu_bytes; byte_index += BLOCK_SIZE * NR_TASKLETS) {
		memset(bitmask_cache, 0, BLOCK_SIZE / sizeof(T) / 32 * sizeof(uint32_t));
		mram_read((__mram_ptr void const*)(mram_base_addr + byte_index), cache, BLOCK_SIZE);
		if (byte_index + (BLOCK_SIZE - 1) >= input_size_dpu_bytes) {
			for (unsigned int i = 0; byte_index + (i * sizeof(T)) < input_size_dpu_bytes; i++) {
				bitmask_cache[i/32] |= (_pred_f(cache[i]) * 1) << (i%32);
			}
		} else {
			for (unsigned int i = 0; i < BLOCK_SIZE / sizeof(T) / 32; i++) {
				for (unsigned int j = 0; j < 32; j++) {
					bitmask_cache[i] |= (_pred_f(cache[i*32+j]) * 1) << j;
				}
			}
		}
		mram_write(bitmask_cache, (__mram_ptr void*)(bitmask_base_addr + byte_index / sizeof(T) / 32 * sizeof(uint32_t)), BLOCK_SIZE / sizeof(T) / 32 * sizeof(uint32_t));
	}

	return 0;
}
