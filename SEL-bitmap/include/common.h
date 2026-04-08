#pragma once

// Transfer size between MRAM and WRAM
#ifdef BL
#define BLOCK_SIZE_LOG2 BL
#define BLOCK_SIZE (1 << BLOCK_SIZE_LOG2)
#else
#define BLOCK_SIZE_LOG2 10
#define BLOCK_SIZE (1 << BLOCK_SIZE_LOG2)
#define BL BLOCK_SIZE_LOG2
#endif

enum kernels {
	kernel_select = 0,
	nr_kernels = 1,
};

typedef struct {
	uint32_t size;
	uint8_t kernel;
	uint8_t padding1;
	uint16_t padding2;
} dpu_arguments_t;
