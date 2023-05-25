
#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdio.h>
#include <sys/time.h>

typedef struct Timer {
    struct timeval startTime[5];
    struct timeval stopTime[5];
    double         time[5];
} Timer;

static void startTimer(Timer *timer, int i, int rep) {
    if(rep == 0) {
        timer->time[i] = 0.0;
    }
    gettimeofday(&timer->startTime[i], NULL);
}

static void stopTimer(Timer *timer, int i) {
    gettimeofday(&timer->stopTime[i], NULL);
    timer->time[i] += (timer->stopTime[i].tv_sec - timer->startTime[i].tv_sec) * 1000000.0 +
                      (timer->stopTime[i].tv_usec - timer->startTime[i].tv_usec);
}

static void printAll(Timer *timer, int maxt) {
    for (int i = 0; i <= maxt; i++) {
        printf(" timer%d_us=%f", i, timer->time[i]);
    }
    printf("\n");
}

#endif
