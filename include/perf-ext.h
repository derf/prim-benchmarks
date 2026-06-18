#include <stdio.h>
#include <string.h>

int perf_start()
{
	char buf[8];

	FILE *ctl = fopen("/tmp/perf-control", "w");
	if (ctl == NULL) {
		perror("fopen /tmp/perf-control");
		return -1;
	}

	if (fputs("enable\n", ctl) == EOF) {
		perror("fputs");
		return -1;
	}

	fclose(ctl);

	FILE *ack = fopen("/tmp/perf-ack", "r");
	if (ack == NULL) {
		perror("fopen /tmp/perf-ack");
		return -1;
	}

	if (fgets(buf, 8, ack) == NULL) {
		perror("fgets");
		return -1;
	}

	fclose(ack);

	if (strncmp(buf, "ack\n", 4) == 0) {
		return 0;
	}
	printf("/tmp/perf-ack: received '%s' (expected 'ack')\n", buf);
	return -1;
}

int perf_stop()
{
	char buf[8];

	FILE *ctl = fopen("/tmp/perf-control", "w");
	if (ctl == NULL) {
		perror("fopen /tmp/perf-control");
		return -1;
	}

	if (fputs("disable\n", ctl) == EOF) {
		perror("fputs");
		return -1;
	}

	fclose(ctl);

	FILE *ack = fopen("/tmp/perf-ack", "r");
	if (ack == NULL) {
		perror("fopen /tmp/perf-ack");
		return -1;
	}

	if (fgets(buf, 8, ack) == NULL) {
		perror("fgets");
		return -1;
	}

	fclose(ack);

	if (strncmp(buf, "ack\n", 4) == 0) {
		return 0;
	}
	printf("/tmp/perf-ack: received '%s' (expected 'ack')\n", buf);
	return -1;
}
