#pragma once
#include <time.h>

typedef struct Timer {

    struct timespec startTime[7];
    struct timespec stopTime[7];
    uint64_t        nanoseconds[7];

} Timer;

void start(Timer *timer, int i, int rep) {
    if(rep == 0) {
        timer->nanoseconds[i] = 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &timer->startTime[i]);
}

void stop(Timer *timer, int i) {
    clock_gettime(CLOCK_MONOTONIC, &timer->stopTime[i]);
    timer->nanoseconds[i] += (timer->stopTime[i].tv_sec - timer->startTime[i].tv_sec) * 1000000000 +
                             (timer->stopTime[i].tv_nsec - timer->startTime[i].tv_nsec);
}
