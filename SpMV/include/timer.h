#pragma once

#include <stdio.h>
#include <sys/time.h>

#if DFATOOL_TIMING

#define dfatool_printf(fmt, ...) do { printf(fmt, __VA_ARGS__); } while (0)

typedef struct Timer {
	struct timeval startTime;
	struct timeval endTime;
} Timer;

static void startTimer(Timer *timer)
{
	gettimeofday(&(timer->startTime), NULL);
}

static void stopTimer(Timer *timer)
{
	gettimeofday(&(timer->endTime), NULL);
}

static double getElapsedTime(Timer timer)
{
	return ((double)((timer.endTime.tv_sec - timer.startTime.tv_sec)
			 + (timer.endTime.tv_usec -
			    timer.startTime.tv_usec) / 1.0e6));
}

#else

#define dfatool_printf(fmt, ...) do {} while (0)

typedef int Timer;

static void startTimer(Timer* timer)
{
	(void)timer;
}

static void stopTimer(Timer* timer)
{
	(void)timer;
}

static double getElapsedTime(Timer timer)
{
	(void)timer;
	return 0.0;
}

#endif
