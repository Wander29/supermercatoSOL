#include <pool.h>

int pool_start(pool_set_t *arg) {
    if(arg == NULL) {
        return -1;
    }
    int err;

    PTHLIB(err, pthread_cond_init(&(arg->cond), NULL))
    PTHLIB(err, pthread_mutex_init(&(arg->mtx), NULL))

    arg->jobs = 0;

    return 0;
}

int pool_destroy(pool_set_t *arg) {
    int err;

    PTHLIB(err, pthread_cond_destroy(&(arg->cond)))
    PTHLIB(err, pthread_mutex_destroy(&(arg->mtx)))
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