#include "../include/notificatore.h"

static int      notificatore_attendi_apertura_cassa(cassa_arg_t *cassa);
static int      notificatore_notifica(cassa_public_t *c, int timeout);

void *notificatore(void *arg) {
    /*******************************************************************
     * NOTIFICATORE
     * avvisa il direttore ad intervalli regolari di ampiezza A
     *  riguardo il numero di clienti in coda
     * INVIA
     *      .tipo messaggio: CASSIERE_NUM_CLIENTI
     *      .indice della coda
     *      .numero clienti in coda
     *
     * Se la cassa viene chiusa si mette in attesa sulla
     *      var. di condizione della cassa
     *******************************************************************/
    cassa_arg_t *Cassa_args = (cassa_arg_t *) arg;
    cassa_public_t *C = Cassa_args->cassa_set;
    cassa_com_arg_t *Com = Cassa_args->shared;

    do {
        if(notificatore_attendi_apertura_cassa(Cassa_args) == 1) {
            goto terminazione_notificatore;
        }

        MENO1(millisleep(500 + Com->A))
        if(notificatore_notifica(C, Com->A) == -1) {
            fprintf(stderr, "ERRORE: notificatore_notifica\n");
            return (void *) -1;
        }

    }  while(get_stato_supermercato() != CHIUSURA_IMMEDIATA);

    terminazione_notificatore:
#ifdef DEBUG_TERM
    printf("[NOTIFICATORE %d] TERMINATO CORRETTAMENTE!\n", C->index);
    fflush(stdout);
#endif
    return (void *) NULL;
}

static int notificatore_notifica(cassa_public_t *c, int timeout) {
    int err,
        num_clienti,
        index = c->index;
    sock_msg_code_t sock_msg = CASSIERE_NUM_CLIENTI;

    for(;;) {
        PTHLIB(err, pthread_mutex_lock(&(c->mtx_cassa))) {
            if(c->stato == CHIUSA) {
                PTHLIB(err, pthread_mutex_unlock(&(c->mtx_cassa)))
                return 0;
            }
            num_clienti = c->q->nelems;
        } PTHLIB(err, pthread_mutex_unlock(&(c->mtx_cassa)))

        MENO1LIB(socket_write(&sock_msg, 2, &index, &num_clienti), -1)
        MENO1(millisleep(timeout))
    }
}

static int notificatore_attendi_apertura_cassa(cassa_arg_t *cassa) {
    int err;
    cassa_public_t *c = cassa->cassa_set;

    PTHLIB(err, pthread_mutex_lock(&(c->mtx_cassa))) {
        while(c->stato != APERTA) {
            PTHLIB(err, pthread_cond_wait(&(cassa->cond_notif), &(c->mtx_cassa)))

            PTHLIB(err, pthread_mutex_unlock(&(c->mtx_cassa)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
                return 1;
            PTHLIB(err, pthread_mutex_lock(&(c->mtx_cassa)))
        }
    } PTHLIB(err, pthread_mutex_unlock(&(c->mtx_cassa)))

    return 0;
}