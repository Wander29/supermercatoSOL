#include "../include/cassiere.h"

static void *notificatore(void *arg);
static int notificatre_attendi_apertura_cassa(cassa_specific_t *c);
static int cassiere_attesa_lavoro(pool_set_t *P);
static int cassiere_pop_cliente(cassa_specific_t *C, ret_pop_t *val);
static int cassiere_sveglia_clienti(queue_t *q, attesa_t stato);

/**********************************************************************************************************
 * LOGICA DEL CASSIERE
 * - viene creato => aspetta che la cassa venga aperta
 * - la prima votla che viene aperta spawna un thread secondario notificatore
 * - attende che ci siano clienti
 * - se ci sono clienti in coda li serve
 *
 * - se è CHIUSA, il manager la può svegliare per:
 *          comunicazione dal direttore -> apre
 *          chiusura supermercato       -> terminazione thread
 * - se è APERTA
 *          se riceve dal direttore di chiudere -> va in attesa, va nello stato CHIUSA
 *          se CHIUSURA IMMEDIATA
 *              -> serve cliente corrente
 *                      avverte tutti i clienti in coda
*                       termina
 *          se CHIUSURA SOFT
 *              -> serve tutti i clienti in coda, quando la coda è vuota terminerà
 *
 **********************************************************************************************************/
void *cassiere(void *arg) {
    int err;

    /****************************************************************************
    * Cassiere _ THREAD
     * - casting degli argomenti
     * - inizializzaione
     *      cond, mutex
     *      coda
    ****************************************************************************/
    cassa_arg_t *Cassa_args = (cassa_arg_t *) arg;
    cassa_specific_t *C = Cassa_args->cassa_set;
    cassa_com_arg_t *Com = Cassa_args->shared;
    pool_set_t *P = Com->pool_set;

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    // MENO1(start_queue(&(C->q)))
    //EQNULL(C->q = start_queue2())

    pthread_t tid_notificatore = -1;
    void *status_notificatore;
    int i = C->index;

    unsigned seedp = i + time(NULL);
    /** parametri del Cassiere */

    int tempo_fisso = (rand_r(&seedp) % (MAX_TEMPO_FISSO - MIN_TEMPO_FISSO + 1)) + MIN_TEMPO_FISSO; // 20 - 80 ms
#ifdef DEBUG_RAND
    printf("[CASSA %d] tempo fisso: [%d]\n", i, tempo_fisso);
#endif

    int tempo_servizio;
    ret_pop_t cli;

    /************************************************
     * Vita del Cassiere
     ************************************************/
    for(;;) {
        /* attende il lavoro */
        if( (err = cassiere_attesa_lavoro(P)) == 1) { // termina
#ifdef DEBUG
            printf("[CASSA %d] terminato per chiusura supemercato, ero sulla JOBS!\n", C->index);
#endif
            goto terminazione_cassiere;
        }
        else if(err < 0)
            fprintf(stderr, "Errore durante l'attesa di un lavoro per la cassa [%d]\n", i);

        /************************************************************************
         * Quando OTTIENE il lavoro
         *  - se è la prima apertura, avvia il thread notificatore associato
         *      else avvisa il notificatore
         *  - apre la cassa e si mette in attesa di clienti
         *************************************************************************/
        if(tid_notificatore == (pthread_t ) -1) {
            PTH(err, pthread_create(&tid_notificatore, NULL, notificatore, arg))
        } else {
            PTH(err, pthread_cond_signal(&(C->cond))) /* signal al notificatore */
        }

        /* setta la cassa APERTA e la rende la più conveniente in cui aspettare */
        set_stato_cassa(C, APERTA);
        set_min_queue(C, 0);

#ifdef DEBUG_CASSIERE
        printf("[CASSA %d] APERTA!\n", C->index);
#endif

        /********************************************************************************
         * GESTIONE CLIENTI(cassa APERTA)
         * - controlla che la cassa non sia stata chiusa || che il supermercato non
         *          stia chiudendo immediatamente
         * - se ci sono clienti, ne serve uno
         * - aggiorna il numero di clienti in coda
         *      SE il supermercato sta chiudendo (NON immediatamente) e il numero di clienti
         *      in attesa di essere serviti nel supermercato è 0;
         *      invia al Manager tale comunicazione
         ********************************************************************************/
        for(;;) {
            if((cassiere_pop_cliente(C, &cli)) == -1) {
                if (SM_IN_CHIUSURA == cli.stato) {
#ifdef DEBUG
                    printf("[CASSA %d] esco, ero sulla COND_READ!\n", C->index);
#endif
                    cassiere_sveglia_clienti(C->q, SM_IN_CHIUSURA);
                    goto terminazione_cassiere;
                }
                else if (CASSA_IN_CHIUSURA == cli.stato) {
#ifdef DEBUG
                    printf("[CASSA %d] chiudo la cassa (potrei riaprirla poi..) \tDIM [%d]!\n", C->index, queue_get_len(C->q));
#endif
                    /* se era la coda più conveniente resetto min_queue */
                    MENO1LIB(is_min_queue_testreset(C), (void *)-1)
                    cassiere_sveglia_clienti(C->q, CASSA_IN_CHIUSURA);
#ifdef DEBUG
                    printf("[CASSA %d] Ho detto che sto in chiusura \tDIM [%d]!\n", C->index, queue_get_len(C->q));
#endif
                    break;
                }
                else {
                    // gestione errore ritorno dalla funzione
                    fprintf(stderr, "ERRORE: cassiere, pop_cliente!\n");
                    return (void *) -1;
                }
            }
            /* ho fatto la POP, controllo se la mia coda `e la piú conveniente */
            MENO1LIB( testset_min_queue(C, queue_get_len(C->q)), (void *) -1)
#ifdef DEBUG_QUEUE
            printf("[CASSA %d] cliente estratto\tDIM[%d]\n", C->index, queue_get_len(C->q));
#endif
            /**************************************************
             * Cliente estratto dalla coda
             * - lo servo
             *      - calcolo il tempo di servizio
             *      - simulo il servizio con una nanosleep
             *      - sveglio il cliente
             *      - decremento il numero di clienti in coda nel supermercato
             **************************************************/
            tempo_servizio = tempo_fisso + cli.ptr->num_prodotti * Com->tempo_prodotto;
            // ts.tv_nsec = nsTOmsMULT * tempo_servizio;
#ifdef DEBUG_RAND
            printf("[CASSA %d] sto per aspettare [%d]ms!\n", C->index, tempo_servizio);
#endif
            //MENO1(nanosleep(&ts, NULL))
            MENO1(millisleep(tempo_servizio))
#ifdef DEBUG
            printf("[CASSA %d] ho servito sto cliente!\n", C->index);
#endif

            NOTZERO((set_stato_attesa(cli.ptr, SERVITO)))
            //cli.ptr->stato_attesa = SERVITO;
            PTH(err, pthread_cond_signal(&(cli.ptr->cond_cl_q)))
        }
    }

terminazione_cassiere:
    NOTZERO(set_stato_cassa(C, CHIUSA))
    PTH(err, pthread_cond_signal(&(C->cond)))

    if(tid_notificatore != (pthread_t) -1) {
        PTH(err, pthread_join(tid_notificatore, &status_notificatore))
        PTHJOIN(status_notificatore, "Notificatore cassiere")
    }

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))

#ifdef DEBUG_TERM
    printf("[CASSIERE %d] TERMINATO CORRETTAMENTE!\n", i);
#endif

    return (void *) 0;
}

stato_cassa_t get_stato_cassa(cassa_specific_t *cassa) {
    int err;
    stato_cassa_t stato;

    PTH(err, pthread_mutex_lock(&(cassa->mtx))) {
        stato = cassa->stato;
    } PTH(err, pthread_mutex_unlock(&(cassa->mtx)))

    return stato;
}

int set_stato_cassa(cassa_specific_t *cassa, const stato_cassa_t s) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(cassa->mtx))) {
        cassa->stato = s;
    } PTHLIB(err, pthread_mutex_unlock(&(cassa->mtx)))

    return 0;
}

static int cassiere_attesa_lavoro(pool_set_t *P) {
    int err;

    PTH(err, pthread_mutex_lock(&(P->mtx))) {
        while(P->jobs == 0){
#ifdef DEBUG_CASSIERE
            printf("[CASSA] Non c'è lavoro!\n");
#endif
            PTH(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
            /*
             * SE la cassa era chiusa, e viene svegliata dal Manager
             * per la chiusura del supermercato => termina
             */
            PTH(err, pthread_mutex_unlock(&(P->mtx)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                return 1;
            }
            PTH(err, pthread_mutex_lock(&(P->mtx)))
#ifdef DEBUG_NOTIFY
            printf("[CASSA] APERTA!\n");
#endif
        }
#ifdef DEBUG_CASSIERE
        printf("[CASSA] sto per aprire!\n");
#endif
        P->jobs--;
#ifdef DEBUG_CASSIERE
        printf("[CASSIERE] JOBS [%d]!\n", P->jobs);
#endif
    } PTH(err, pthread_mutex_unlock(&(P->mtx)))

    return 0;
}

static void *notificatore(void *arg) {
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
    cassa_specific_t *C = Cassa_args->cassa_set;
    cassa_com_arg_t *Com = Cassa_args->shared;

#ifdef DEBUG_NOTIFY
    printf("[NOTIFICATORE %d] sono nato!\n", C->index);
#endif
    sock_msg_code_t sock_msg = CASSIERE_NUM_CLIENTI;
    int num_clienti,
        index = C->index;

    do {
#ifdef DEBUG_NOTIFY
        printf("[NOTIFICATORE %d] attendo stato!\n", C->index);
#endif
        if(notificatre_attendi_apertura_cassa(C) == 1) {
            goto terminazione_notificatore;
        }
#ifdef DEBUG_NOTIFY
        printf("[NOTIFICATORE %d] ATTIVO!\n", C->index);
#endif
        MENO1(millisleep(1000 + Com->A))
        while(get_stato_cassa(C) == APERTA) {
            MENO1(num_clienti = queue_get_len(C->q))
            socket_write(&sock_msg, 2, &index, &num_clienti);
            MENO1(millisleep(Com->A))
        }

    }  while(get_stato_supermercato() != CHIUSURA_IMMEDIATA);


terminazione_notificatore:
#ifdef DEBUG_NOTIFY
    printf("[NOTIFICATORE %d] TERMINATO CORRETTAMENTE!\n", C->index);
    fflush(stdout);
#endif
    return (void *) NULL;
}

static int cassiere_pop_cliente(cassa_specific_t *C, ret_pop_t *val) {

    CASSA_APERTA_CHECK(C)
    queue_t *q = C->q;
    int err;          // retval, per errori

    PTHLIB(err, pthread_mutex_lock(&(C->mtx))) {
        while(q->nelems == 0) {
#ifdef DEBUG_CASSIERE
            printf("[CASSIERE] Non c'è gente!\n");
#endif
            PTHLIB(err, pthread_cond_wait(&(q->cond_read), &(C->mtx)))

            PTHLIB(err, pthread_mutex_unlock(&(C->mtx)))

            CASSA_APERTA_CHECK(C)

            PTHLIB(err, pthread_mutex_lock(&(C->mtx)))
        }
        val->ptr = (queue_elem_t *) get_from_queue(q);
        if(val->ptr == NULL)
            fprintf(stderr, "NO elements in Queue: %s\n", __func__);
        val->ptr->stato_attesa = SERVIZIO_IN_CORSO;
    } PTHLIB(err, pthread_mutex_unlock(&(C->mtx)))

    return 0;
}

static int cassiere_sveglia_clienti(queue_t *q, attesa_t stato) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(q->mtx_queue))) {
        queue_elem_t *curr;
#ifdef DEBUG_MUTEX
        printf("[CASSIERE] Mutex LOCK![%p]\n", (void *) &(q->mtx_queue));
#endif
#ifdef DEBUG_WAKE
        printf("[CASSA] DIM pre-wake[%d]\n", q->nelems);
#endif
        while(q->nelems > 0) {
            curr = (queue_elem_t *) get_from_queue(q);
            if(curr == NULL) {
                fprintf(stderr, "NO elements in Queue: %s\n", __func__);
                PTHLIB(err, pthread_mutex_unlock(&(q->mtx_queue)))
                return 0;
            }
#ifdef DEBUG_a
            printf("[CASSIERE] sveglio con prodotti [%d]!\n", curr->num_prodotti);
#endif
            //PTHLIB(err, pthread_mutex_unlock(&(q->mtx_queue)))
            //NOTZERO(set_stato_attesa(curr, stato))
            curr->stato_attesa = stato;
            PTHLIB(err, pthread_cond_signal(&(curr->cond_cl_q)))
            //PTHLIB(err, pthread_mutex_lock(&(q->mtx_queue)))
        }
    }
#ifdef DEBUG_WAKE
    printf("[CASSA] DIM post-wake[%d]\n", q->nelems);
#endif
    PTHLIB(err, pthread_mutex_unlock(&(q->mtx_queue)))
#ifdef DEBUG_MUTEX
    printf("[CASSIERE] Mutex UNLOCK![%p]\n", (void *) &(q->mtx_queue));
#endif
    return 0;
}

static int notificatre_attendi_apertura_cassa(cassa_specific_t *c) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(c->mtx))) {
        while(c->stato != APERTA) {
            PTHLIB(err, pthread_cond_wait(&(c->cond), &(c->mtx)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                PTHLIB(err, pthread_mutex_unlock(&(c->mtx)))
                return 1;
            }
        }
    } PTHLIB(err, pthread_mutex_unlock(&(c->mtx)))

    return 0;
}
