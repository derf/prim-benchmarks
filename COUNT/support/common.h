#ifndef _COMMON_H_
#define _COMMON_H_

// Structures used by both the host and the dpu to communicate information 
typedef struct {
	uint32_t size;
	enum kernels {
		kernel1 = 0,
		nr_kernels = 1,
	} kernel;
} dpu_arguments_t;

typedef struct {
	uint32_t t_count;
} dpu_results_t;

// Transfer size between MRAM and WRAM
#ifdef BL
#define BLOCK_SIZE_LOG2 BL
#define BLOCK_SIZE (1 << BLOCK_SIZE_LOG2)
#else
#define BLOCK_SIZE_LOG2 8
#define BLOCK_SIZE (1 << BLOCK_SIZE_LOG2)
#define BL BLOCK_SIZE_LOG2
#endif

// Data type
#define T uint64_t
#define REGS (BLOCK_SIZE >> 3)	// 64 bits

// Sample predicate
bool pred(const T x)
{
	return (x % 2) == 0;
}

#ifndef ENERGY
#define ENERGY 0
#endif
#define PRINT 0

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#define divceil(n, m) (((n)-1) / (m) + 1)
#define roundup(n, m) ((n / m) * m + m)
#endif
