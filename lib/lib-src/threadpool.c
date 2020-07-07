#include <threadpool.h>

/**********************************************************************************************************
 * LOGICA DELL'EXECUTOR
 * - attende di dover lavorare su una var. di condizione comune all'intero thread_pool
 * - una volta ottenuto il lavoro
 *      - decrementa il numero di jobs disponibili
 *      - chiama la funzione per cui è designato
 **********************************************************************************************************/
void *executor(void *arg) {
#ifdef DEBUG
    printf("sono nell'EXECUTOR!\n");
#endif

    int err;
    threadpool_set_t *P = (threadpool_set_t *) arg;
    if(pthread_mutex_lock(&(P->mtx)) != 0) {
        perror("lock");
        return (void *) -1;
    }
    while(P->jobs == 0) {
        if(pthread_cond_wait(&(P->cond), &(P->mtx)) != 0) {
            perror("wait");
            return (void *) -1;
        }
        if(P->termina == 1) {
            if(pthread_mutex_unlock(&(P->mtx)) != 0) {
                perror("unlock");
                return (void *) -1;
            }
            return 0;
        }
    }

    P->jobs--;
    if(pthread_mutex_unlock(&(P->mtx)) != 0) {
        perror("unlock");
        return (void *) -1;
    }

    return (P->fun(P->fun_arg));
}

int pool_start(threadpool_set_t *arg) {
/*
 *
    arg.jobs = par.J;
    arg.fun = cassiere;
    arg.fun_arg = (void *)32;
 */
    if(err, pthread_cond_init(&(arg->cond), NULL))
    PTH(err, pthread_mutex_init(&(arg->mtx), NULL))
    jobs = 0;
    termina = 0;
    void *(*fun)(void *);
    void *fun_arg;
}
