#include <protocollo.h>

stato_sm_t stato_supermercato = APERTO;
pthread_mutex_t mtx_stato_supermercato = PTHREAD_MUTEX_INITIALIZER;

stato_sm_t get_stato_supermercato(void) {
    int err;
    stato_sm_t ret;

    PTHLIB(err, pthread_mutex_lock(&mtx_stato_supermercato)) {
        ret = stato_supermercato;
    } PTHLIB(err, pthread_mutex_unlock(&mtx_stato_supermercato))

    return ret;
}

int set_stato_supermercato(const stato_sm_t x) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&mtx_stato_supermercato)) {
        stato_supermercato = x;
    }
    PTHLIB(err, pthread_mutex_unlock(&mtx_stato_supermercato))

    return 0;
}