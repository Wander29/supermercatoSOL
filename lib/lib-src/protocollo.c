#include <protocollo.h>
#include <sys/un.h>

stato_sm_t stato_supermercato = APERTO;
pthread_mutex_t mtx_stato_supermercato = PTHREAD_MUTEX_INITIALIZER;
int num_clienti_in_coda = 0;

stato_sm_t get_stato_supermercato() {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_stato_supermercato))
    stato_sm_t r = stato_supermercato;
    PTH(err, pthread_mutex_unlock(&mtx_stato_supermercato))

    return r;
}

void set_stato_supermercato(const stato_sm_t x) {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_stato_supermercato))
    stato_supermercato = x;
    PTH(err, pthread_mutex_unlock(&mtx_stato_supermercato))
}

int dec_num_clienti_in_coda() {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_stato_supermercato));
    int r = --num_clienti_in_coda;
    PTH(err, pthread_mutex_unlock(&mtx_stato_supermercato))

    return r;
}

int inc_num_clienti_in_coda() {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_stato_supermercato));
    int r = ++num_clienti_in_coda;
    PTH(err, pthread_mutex_unlock(&mtx_stato_supermercato))

    return r;
}
