#include "../include/cliente.h"

static int cliente_attesa_lavoro(pool_set_t *P);
static int cliente_attendi_permesso_uscita(cliente_arg_t *t);
static int cliente_push(cassa_specific_t *C, queue_elem_t *new_elem);
static attesa_t cliente_attendi_servizio(queue_elem_t *e);

inline static int cliente_cassa_random(unsigned *seedptr, int k, cassa_specific_t *c) {
    int x;

    do {
        x = rand_r(seedptr) % k;
#ifdef DEBUG_RAND
        printf("[CLIENTE] random X[%d] K[%d]!\n", x, k);
#endif
        if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
            return -1;
    } while(get_stato_cassa(c+x) != APERTA);

    return x;
}

void *cliente(void *arg) {
    int p = 0,
        t,
        err;
    int running = 0;

    /****************************************************************************
    * Cliente _ THREAD
     * - casting degli argomenti
     * - inizializzaione
     *      cond, mutex
     * - ATTESA LAVORO
     *      - se il supermercato e in chiusura (SOFT o IMMEDIATA)
     *              viene svegliato, e deve terminare
     * - entra nel supermercato
     *      avvisa il manager (il quale incrementerà il numero di clienti attivi nel supermercato)
     * - fa acquisti
     *      - numero prodotti IN [0, P]
     *      - tempo acquisti IN [10, T]ms
     * - se il supermercato viene chiuso
     *      - termina
     * - se non acquista nulla
     *      chiede il permesso per uscire
     *          una volta ricevuto, termina
     * - sceglie una cassa random
     *      SE lo stato del supermercato è: chiusura IMM.
     *          termina
     * - quando viene svegliato dall'attesa in una cassa:
     *      SE la cassa è chiusa => trova una nuova cassa aperta
     *      SE il supermercato sta chiudendo, termina
     * - finge di uscire dal supermercato
     *      avvisa il manager che sta uscendo (il quale decrementerà il numero di clienti attivi nel supermercato)
     *      SE il supermercato è aperto => torna ad ATTESA LAVORO
     *      ELSE termina
    ****************************************************************************/
    cliente_arg_t *C = (cliente_arg_t *) arg;
    pool_set_t *P = C->shared->pool_set;
    cassa_specific_t *Casse = C->shared->casse;

#ifdef DEBUG_CLIENTE
    printf("[CLIENTE %d] sono nato!\n", C->index);
#endif

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    queue_elem_t elem;

    PTH(err, pthread_cond_init(&(elem.cond_cl_q), NULL))
    PTH(err, pthread_mutex_init(&(elem.mtx_cl_q), NULL))

    pipe_msg_code_t type_msg;
    unsigned seedp = C->index + time(NULL);
    attesa_t stato_att;

    for(;;) {
        /* attende il lavoro */
        if ((err = cliente_attesa_lavoro(P)) == 1) { // termina
#ifdef DEBUG_TERM
            printf("[CLIENTE %d] terminato per chiusura supermercato, ero sulla JOBS!\n", C->index);
#endif
            goto terminazione_cliente;
        } else if (err < 0)
            fprintf(stderr, "Errore durante l'attesa di un lavoro per il Cliente [%d]\n", C->index);

        /* lavoro ottenuto! */
        type_msg = CLIENTE_ENTRATA;
        pipe_write(&type_msg, NULL);

        /* reinizializzazione valori */
        running = 1;
        NOTZERO(set_permesso_uscita(C, 0))
        NOTZERO(set_stato_attesa(&elem, IN_ATTESA))
        /*
         * inizializzazione valori casuali
         */
        p = rand_r(&seedp) % (C->shared->P + 1);
        t = (rand_r(&seedp) % (C->shared->T - MIN_TEMPO_ACQUISTI + 1)) + MIN_TEMPO_ACQUISTI;
#ifdef DEBUG_RAND
        printf("[CLIENTE %d] p: [%d]\tt: [%d]\n",C->index, p, t);
#endif
        /*
         * fa acquisti
         */
        // struct timespec ts = {0, 0};
        // ts.tv_nsec = nsTOmsMULT * t;
#ifdef DEBUG_RAND
        printf("[CLIENTE %d] comincio gli acquisti! attesa [%d]ms\n", C->index, t);
#endif
        //MENO1(nanosleep(&ts, NULL))
        MENO1(millisleep(t))
#ifdef DEBUG_RAND
        printf("[CLIENTE %d] Nanosleep passata\n", C->index);
#endif
        if (get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
#ifdef DEBUG_TERM
            printf("[CLIENTE %d] terminato per chiusura supermercato, dopo acquisti!\n", C->index);
#endif
            goto terminazione_cliente;
        }

        if (p == 0) {
        /**********************************************************
         * CLIENTE: uscita senza acquisti
         * vuole comunicare al direttore di voler uscire:
         *      scrive sulla pipe in mutua esclusione la sua richiesta,
         *          (completa di indice cliente)
         *      e si mette in attesa di una risposta
         **********************************************************/
#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Non ho fatto acquisti, voglio uscire!\n", C->index);
#endif
        type_msg = CLIENTE_RICHIESTA_PERMESSO;
        MENO1(pipe_write(&type_msg, &(C->index)))

        NOTZERO(cliente_attendi_permesso_uscita(C))
        } else {
            /**********************************************************
            * CLIENTE: pagamento in cassa
            * - sceglie una cassa random
            *      SE lo stato è: chiusura_supermercato
            *          termina
            ***********************************************************/
            elem.num_prodotti = p;

            int x = cliente_cassa_random(&seedp, C->shared->numero_casse, Casse);
            if(x == -1) {
#ifdef DEBUG_TERM
                printf("[CLIENTE %d] terminato per chiusura supemercato, dopo scelta cassa random!\n", C->index);
#endif
                goto terminazione_cliente;
            }
#ifdef DEBUG_CASSIERE
            printf("[CLIENTE %d] sto per pusharmi nella coda [%d]!\n", C->index, x);
#endif
            err = -1;
            MENO1LIB(cliente_push(Casse+x, &elem), ((void *) -1))

            stato_att = cliente_attendi_servizio(&elem);
            while (stato_att != SERVITO) {
#ifdef DEBUG_CASSIERE
                printf("[CLIENTE %d] NON sono stato servito!\n", C->index);
#endif
                /******************************************
                 * Cliente è stato svegliato dalla coda MA
                 *  NON è stato servito
                 ******************************************/
                if(stato_att == CASSA_IN_CHIUSURA) { // si accoda ad un'altra cassa random
                    int y;
                    do {
                        y = cliente_cassa_random(&seedp, C->shared->numero_casse, Casse);
                    } while (x == y);

                    MENO1LIB(cliente_push(Casse + y, &elem), ((void *) -1))
                } else if(stato_att  == SM_IN_CHIUSURA) {
#ifdef DEBUG_TERM
                    printf("[CLIENTE %d] terminato per chiusura supermercato, ero in CODA!\n", C->index);
#endif
                    goto terminazione_cliente;
                }
            }
            if (err < 0){
                fprintf(stderr, "ERRORE [CLIENTE] durante attesa del servizio\n");
                goto terminazione_cliente;
            }

        }
#ifdef DEBUG_TERM
        printf("[CLIENTE %d] Rivivrò?\n", C->index);
#endif
        running = 0;
        /*******************************************
         * Cliente SERVITO
         * - comunica l'uscita dal supermercato (potrebbe rientrare come thread)
         * - se il supermercato sta chiudendo, il thread TERMINA
         ********************************************/
        type_msg = CLIENTE_USCITA;

        pipe_write(&type_msg, NULL);

        if(get_stato_supermercato() != APERTO) {
#ifdef DEBUG_TERM
            printf("[CLIENTE %d] Sono stato servito, vado a casa ora!\n", C->index);
#endif
            break;
        }
    }


terminazione_cliente:
    if(running == 1) {
        type_msg = CLIENTE_USCITA;
        pipe_write(&type_msg, NULL);
    }

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))

    PTH(err, pthread_mutex_destroy(&(elem.mtx_cl_q)))
    PTH(err, pthread_cond_destroy(&(elem.cond_cl_q)))

#ifdef DEBUG_TERM
    printf("[CLIENTE %d] TERMINATO CORRETTAMENTE\n", C->index);
#endif
    return ((void *)NULL);
}

static int cliente_attesa_lavoro(pool_set_t *P) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(P->mtx)))
    while(P->jobs == 0) {
#ifdef DEBUG_WAIT
        printf("[CLIENTE] sto andando in wait JOBS!\n");
#endif
        PTHLIB(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
        PTHLIB(err, pthread_mutex_unlock(&(P->mtx)))
        /* se è stato svegliato perchè il supermercato è in chiusura => deve terminare */
        if(get_stato_supermercato() != APERTO) {
            PTHLIB(err, pthread_mutex_unlock(&(P->mtx)))
            return 1; // anche se la chiusura è SOFT!
        }
        PTHLIB(err, pthread_mutex_lock(&(P->mtx)))
    }
    P->jobs--;
#ifdef DEBUG_WAIT
    printf("[CLIENTE] JOBS [%d]!\n", P->jobs);
#endif
    PTHLIB(err, pthread_mutex_unlock(&(P->mtx)))

    return 0;
}

static attesa_t cliente_attendi_servizio(queue_elem_t *e) {
    int err;
    attesa_t ret;

    PTHLIB(err, pthread_mutex_lock(&(e->mtx_cl_q))) {
        while(e->stato_attesa == IN_ATTESA) {
#ifdef DEBUG_WAIT
            printf("[CLIENTE] sto andando in wait CODA!\n");
#endif
            PTHLIB(err, pthread_cond_wait(&(e->cond_cl_q), &(e->mtx_cl_q)))
            ret = e->stato_attesa;
        }
    } PTHLIB(err, pthread_mutex_unlock(&(e->mtx_cl_q)))

    return ret;
}

static int cliente_attendi_permesso_uscita(cliente_arg_t *t) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(t->mtx))) {
        while(t->permesso_uscita == 0)
                PTHLIB(err, pthread_cond_wait(&(t->cond), &(t->mtx)))
        if(err == -1) {
            PTHLIB(err, pthread_mutex_unlock(&(t->mtx)))
            return -1;
        }

#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Permesso di uscita ricevuto!\n", t->index);
#endif
    } PTHLIB(err, pthread_mutex_unlock(&(t->mtx)))

    return 0;
}

int cliente_push(cassa_specific_t *C, queue_elem_t *new_elem) {
    int r;          // retval, per errori
    queue_t *q = C->q;
    PTHLIB(r, pthread_mutex_lock(&(q->mtx_queue))) {
#ifdef DEBUG_CLIENTE
        printf("[CLIENTE] Coda LOCKATA!\n");
#endif
        if(C->stato == CHIUSA) {
            fprintf(stderr, "[CLIENTE] inserimento in una coda chiusa!\n");
            PTHLIB(r, pthread_mutex_unlock(&(q->mtx_queue)))
#ifdef DEBUG_CLIENTE
            printf("[CLIENTE] Coda UNLOCKATA!\n");
#endif
            return -1;
        }

        if(insert_into_queue(q, (void *) new_elem) == -1) {
            PTHLIB(r, pthread_mutex_unlock(&(q->mtx_queue)))
#ifdef DEBUG_CLIENTE
            printf("[CLIENTE] Coda UNLOCKATA!\n");
#endif
            fprintf(stderr, "[CLIENTE] inserimento fallito: %s\n", __func__);
            return -1;
            return -1;
        }
        PTHLIB(r, pthread_cond_signal(&(q->cond_read)))
    } PTHLIB(r, pthread_mutex_unlock(&(q->mtx_queue)))
#ifdef DEBUG_CLIENTE
    printf("[CLIENTE] Coda UNLOCKATA!\n");
#endif
    return 0;
}

int get_permesso_uscita(cliente_arg_t *c) {
    int err,
        ret;

    PTHLIB(err, (pthread_mutex_lock(&(c->mtx)))) {
        ret = c->permesso_uscita;
    } PTHLIB(err, (pthread_mutex_unlock(&(c->mtx))))

    return ret;
}

int set_permesso_uscita(cliente_arg_t *c, const int new_perm) {
    int err;

    PTHLIB(err, (pthread_mutex_lock(&(c->mtx)))) {
        c->permesso_uscita = new_perm;
    } PTHLIB(err, (pthread_mutex_unlock(&(c->mtx))))

    return 0;
}

int set_stato_attesa(queue_elem_t *e, const attesa_t new_stato) {
    int err;

    PTHLIB(err, (pthread_mutex_lock(&(e->mtx_cl_q)))) {
        e->stato_attesa = new_stato;
    } PTHLIB(err, (pthread_mutex_unlock(&(e->mtx_cl_q))))

    return 0;
}

attesa_t get_stato_attesa(queue_elem_t *e) {
    int err;
    attesa_t ret;

    PTHLIB(err, (pthread_mutex_lock(&(e->mtx_cl_q)))) {
        ret = e->stato_attesa;
    } PTHLIB(err, (pthread_mutex_unlock(&(e->mtx_cl_q))))

    return ret;
}