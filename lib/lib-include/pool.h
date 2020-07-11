#ifndef PROGETTO_EXECUTOR_H
#define PROGETTO_EXECUTOR_H

#include <pthread.h>
#include <mypthread.h>
#include <stdio.h>

typedef struct pool_set {
    pthread_cond_t      cond;
    pthread_mutex_t     mtx;
    int jobs;
} pool_set_t;

int pool_destroy(pool_set_t *arg);
int pool_start(pool_set_t *arg);

int get_jobs(pool_set_t *p);
int ch_jobs(pool_set_t *p, const int change);

#endif //PROGETTO_EXECUTOR_H
