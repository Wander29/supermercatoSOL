#include <pool.h>
#include "../lib-include/pool.h"

int pool_start(pool_set_t *arg) {
    if(arg == NULL)
        return -1;

    if(pthread_cond_init(&(arg->cond), NULL) != 0) {
        perror("cond_init");
        return -1;
    }
    if(pthread_mutex_init(&(arg->mtx), NULL) != 0) {
        perror("mutex_init");
        return -1;
    }
    arg->jobs = 0;

    return 0;
}

int pool_destroy(pool_set_t *arg) {
    if(pthread_cond_destroy(&(arg->cond)) != 0)
        return -1;
    if(pthread_mutex_destroy(&(arg->mtx)) != 0)
        return -1;
    arg->jobs = 0;

    return 0;
}

int get_jobs(pool_set_t *p) {
    int err,
        ret;

    PTHLIB(err, pthread_mutex_lock(&(p->mtx))) {
        ret = p->jobs;
    } PTHLIB(err, pthread_mutex_unlock(&(p->mtx)))

    return ret;
}

int ch_jobs(pool_set_t *p, const int change) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(p->mtx))) {
        p->jobs += change;
    } PTHLIB(err, pthread_mutex_unlock(&(p->mtx)))

    return 0;
}

int set_jobs(pool_set_t *p, const int new_val) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(p->mtx))) {
        p->jobs += new_val;
    } PTHLIB(err, pthread_mutex_unlock(&(p->mtx)))

    return 0;
}