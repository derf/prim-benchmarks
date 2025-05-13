#pragma once

#include <sys/time.h>

#if DFATOOL_TIMING

typedef struct Timer {

	struct timeval startTime[N_TIMERS];
	struct timeval stopTime[N_TIMERS];
	double time[N_TIMERS];

} Timer;

#define dfatool_printf(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)

void start(Timer *timer, int i, int rep)
{
	if (rep == 0) {
		timer->time[i] = 0.0;
	}
	gettimeofday(&timer->startTime[i], NULL);
}

void stop(Timer *timer, int i)
{
	gettimeofday(&timer->stopTime[i], NULL);
	timer->time[i] +=
	    (timer->stopTime[i].tv_sec -
	     timer->startTime[i].tv_sec) * 1000000.0 +
	    (timer->stopTime[i].tv_usec - timer->startTime[i].tv_usec);
}

#else

#define dfatool_printf(fmt, ...) do {} while (0)

typedef int Timer;

void start(Timer *timer, int i, int rep)
{
	(void)timer;
	(void)i;
	(void)rep;
}

void stop(Timer *timer, int i)
{
	(void)timer;
	(void)i;
}

#endif
