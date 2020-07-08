#include <pool.h>

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
