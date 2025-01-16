/*
* Count with multiple tasklets
*
*/
#include <stdint.h>
#include <stdio.h>
#include <defs.h>
#include <mram.h>
#include <alloc.h>
#include <perfcounter.h>
#include <handshake.h>
#include <barrier.h>

#include "../support/common.h"

__host dpu_arguments_t DPU_INPUT_ARGUMENTS;
__host dpu_results_t DPU_RESULTS[NR_TASKLETS];

// Array for communication between adjacent tasklets
uint32_t message[NR_TASKLETS];
uint32_t message_partial_count;

// COUNT in each tasklet
static unsigned int count(T *input)
{
	unsigned int cnt = 0;
#pragma unroll
	for (unsigned int j = 0; j < REGS; j++) {
		if (!pred(input[j])) {
			cnt++;
		}
	}
	return cnt;
}

// Handshake with adjacent tasklets
static unsigned int handshake_sync(unsigned int l_count,
				   unsigned int tasklet_id)
{
	unsigned int p_count;
	// Wait and read message
	if (tasklet_id != 0) {
		handshake_wait_for(tasklet_id - 1);
		p_count = message[tasklet_id];
	} else {
		p_count = 0;
	}
	// Write message and notify
	if (tasklet_id < NR_TASKLETS - 1) {
		message[tasklet_id + 1] = p_count + l_count;
		handshake_notify();
	}
	return p_count;
}

// Barrier
BARRIER_INIT(my_barrier, NR_TASKLETS);

extern int main_kernel1(void);

int (*kernels[nr_kernels])(void) = { main_kernel1 };

int main(void)
{
	// Kernel
	return kernels[DPU_INPUT_ARGUMENTS.kernel] ();
}

// main_kernel1
int main_kernel1()
{
	unsigned int tasklet_id = me();
#if PRINT
	printf("tasklet_id = %u\n", tasklet_id);
#endif
	if (tasklet_id == 0) {	// Initialize once the cycle counter
		mem_reset();	// Reset the heap
	}
	// Barrier
	barrier_wait(&my_barrier);

	dpu_results_t *result = &DPU_RESULTS[tasklet_id];

	uint32_t input_size_dpu_bytes = DPU_INPUT_ARGUMENTS.size;

	// Address of the current processing block in MRAM
	uint32_t base_tasklet = tasklet_id << BLOCK_SIZE_LOG2;
	uint32_t mram_base_addr_A = (uint32_t) DPU_MRAM_HEAP_POINTER;

	// Initialize a local cache to store the MRAM block
	T *cache_A = (T *) mem_alloc(BLOCK_SIZE);

	// Initialize shared variable
	if (tasklet_id == NR_TASKLETS - 1)
		message_partial_count = 0;
	// Barrier
	barrier_wait(&my_barrier);

	for (unsigned int byte_index = base_tasklet;
	     byte_index < input_size_dpu_bytes;
	     byte_index += BLOCK_SIZE * NR_TASKLETS) {

		// Load cache with current MRAM block
		mram_read((__mram_ptr void const *)(mram_base_addr_A +
						    byte_index), cache_A,
			  BLOCK_SIZE);

		// COUNT in each tasklet
		uint32_t l_count = count(cache_A);

		// Sync with adjacent tasklets
		uint32_t p_count = handshake_sync(l_count, tasklet_id);

		// Barrier
		barrier_wait(&my_barrier);

		// Total count in this DPU
		if (tasklet_id == NR_TASKLETS - 1) {
			result->t_count =
			    message_partial_count + p_count + l_count;
			message_partial_count = result->t_count;
		}

	}

	return 0;
}
