#pragma once

#include <numa.h>
#include <numaif.h>

void* mp_pages[1];
int mp_status[1];
int mp_nodes[1];
int numa_node_in = -1;
int numa_node_out = -1;
int numa_node_cpu = -1;

void* numa_bind_alloc(size_t size, struct bitmask* bitmask)
{
	if (bitmask != NULL) {
		numa_set_membind(bitmask);
	}
	return numa_alloc(size);
}

int numa_cpu_bind(int node)
{
	if (node != -1) {
		if (numa_run_on_node(node) == -1) {
			perror("numa_run_on_node");
			return -1;
		}
	}
	return node;
}

void numa_mem_unbind()
{
	struct bitmask* bitmask_all = numa_allocate_nodemask();
	numa_bitmask_setall(bitmask_all);
	numa_set_membind(bitmask_all);
	numa_free_nodemask(bitmask_all);
}

int numa_get_node_of_page(void* data, const char* identifier)
{
	mp_pages[0] = data;
	if (move_pages(0, 1, mp_pages, NULL, mp_status, 0) == -1) {
		char desc[64];
		snprintf(desc, sizeof(desc) / sizeof(desc[0]), "move_pages(%s)", identifier);
		perror(desc);
	} else if (mp_status[0] < 0) {
		printf("move_pages error: status byte for '%s' is %d", identifier, mp_status[0]);
	} else {
		return mp_status[0];
	}
	return -1;
}
