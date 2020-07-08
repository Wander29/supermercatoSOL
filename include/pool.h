#ifndef PROGETTO_EXECUTOR_H
#define PROGETTO_EXECUTOR_H

#include <pthread.h>
#include <stdio.h>

typedef struct pool_set {
    pthread_cond_t      cond;
    pthread_mutex_t     mtx;
    unsigned jobs;
} pool_set_t;

int pool_destroy(pool_set_t *arg);
int pool_start(pool_set_t *arg);

#endif //PROGETTO_EXECUTOR_H
