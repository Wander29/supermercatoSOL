#ifndef PROGETTO_THREADPOOL_H
#define PROGETTO_THREADPOOL_H

typedef struct threadpool_set {
    pthread_cond_t      cond;
    pthread_mutex_t     mtx;
    unsigned jobs;
    int termina;
    void *(*fun)(void *);
    void *fun_arg;
} threadpool_set_t;

void *executor(void *arg);

#endif //PROGETTO_THREADPOOL_H
