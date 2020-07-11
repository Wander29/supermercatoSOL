#include "../include/cassiere.h"

static void *notificatore(void *arg);
static int cassiere_attesa_lavoro(pool_set_t *P);
static queue_elem_t *cassiere_pop_cliente(cassa_specific_t *C);
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
    pool_set_t *P = Cassa_args->pool_set;

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    MENO1(start_queue(&(C->q)))

    pthread_t tid_notificatore = -1;
    void *status_notificatore;
    queue_elem_t *cliente;
    int i = C->index;

    /** parametri del Cassiere */
    unsigned seedp = 0;
    int tempo_fisso = (rand_r(&seedp) % (MAX_TEMPO_FISSO - MIN_TEMPO_FISSO + 1)) + MIN_TEMPO_FISSO; // 20 - 80 ms
    int tempo_servizio;
    struct timespec ts = {0, 0};

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

        /* ha ottenuto il lavoro
         *  - se è la prima apertura, avvia il thread notificatore associato
         *  - apre la cassa e si mette in attesa di clienti */
        if(tid_notificatore == (pthread_t ) -1) {
            PTH(err, pthread_create(&tid_notificatore, NULL, notificatore, (void *) NULL))
        }

        set_stato_cassa(C, APERTA);

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
            if( (cliente = cassiere_pop_cliente(C)) == (queue_elem_t *) 1 ) {
                if(CHIUSA == get_stato_cassa(C)) {
#ifdef DEBUG
                    printf("[CASSA %d] chiudo la cassa (potrei riaprirla poi..)!\n", C->index);
#endif
                    cassiere_sveglia_clienti(C->q, CASSA_IN_CHIUSURA);
                    break;
                } else if(CHIUSURA_IMMEDIATA == get_stato_supermercato()) {
#ifdef DEBUG
                    printf("[CASSA %d] esco, ero sulla COND_READ!\n", C->index);
#endif
                    cassiere_sveglia_clienti(C->q, SUPERMERCATO_IN_CHIUSURA);
                    goto terminazione_cassiere;
                }
            } else if(cliente == (queue_elem_t *) -1) {
                // gestione errore ritorno dalla funzione
                fprintf(stderr, "ERRORE: cassiere, pop_cliente\n");
                return (void *) -1;
            }
            /**************************************************
             * Cliente estratto dalla coda
             * - lo servo
             *      - calcolo il tempo di servizio
             *      - simulo il servizio con una nanosleep
             *      - sveglio il cliente
             *      - decremento il numero di clienti in coda nel supermercato
             **************************************************/
            tempo_servizio = tempo_fisso + cliente->num_prodotti * Cassa_args->tempo_prodotto;
            ts.tv_nsec = 1000000 * tempo_servizio;
#ifdef DEBUG
            printf("[CASSA %d] sto per aspettare [%ld]ns!\n", C->index, ts.tv_nsec);
#endif
            MENO1(nanosleep(&ts, NULL))
#ifdef DEBUG
            printf("[CASSA %d] ho servito sto cliente!\n", C->index);
#endif
            cliente->stato_attesa = SERVITO;
            PTH(err, pthread_cond_signal(&(cliente->cond_cl_q)))

            if(dec_num_clienti_in_coda() == 0 && get_stato_supermercato() == CHIUSURA_SOFT) {
                pipe_msg_code_t msg = CLIENTI_TERMINATI;
                pipe_write(&msg, NULL);
            }
        }
#ifdef DEBUG
        printf("[CASSA %d] ucito dal FOR dei clienti!\n", C->index);
#endif
    }

terminazione_cassiere:
    if(tid_notificatore != (pthread_t) -1) {
        PTH(err, pthread_join(tid_notificatore, &status_notificatore))
        PTHJOIN(status_notificatore, "Notificatore cassiere")
    }

    free_queue(C->q, NO_DYNAMIC_ELEMS);

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))


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

void set_stato_cassa(cassa_specific_t *cassa, const stato_cassa_t s) {
    int err;

    PTH(err, pthread_mutex_lock(&(cassa->mtx))) {
        cassa->stato = s;
    } PTH(err, pthread_mutex_unlock(&(cassa->mtx)))

}

static int cassiere_attesa_lavoro(pool_set_t *P) {
    int err;

    PTH(err, pthread_mutex_lock(&(P->mtx))) {
        while(P->jobs == 0){
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
        }
        P->jobs--;
#ifdef DEBUG_CASSIERE
        printf("[CASSIERE] sto per aprire!\n");
#endif
    } PTH(err, pthread_mutex_unlock(&(P->mtx)))

    return 0;
}

static void *notificatore(void *arg) {
#ifdef DEBUG_CLIENTE
    printf("[NOTIFICATORE] sono nato!\n");
#endif
    return (void *) NULL;
}

queue_elem_t *cassiere_pop_cliente(cassa_specific_t *C) {
    CASSA_APERTA_CHECK(C)
    queue_t *q = C->q;
    int err;          // retval, per errori
    queue_elem_t *val;

    PTH(err, pthread_mutex_lock(&(q->mtx))) {
        while(q->nelems == 0) {
#ifdef DEBUG_CASSIERE
            printf("[CASSIERE] Non c'è gente!\n");
#endif
            PTH(err, pthread_cond_wait(&(q->cond_read), &(q->mtx)))

            PTH(err, pthread_mutex_unlock(&(q->mtx)))

            CASSA_APERTA_CHECK(C)
        }
        val = (queue_elem_t *) get_from_queue(q);
        if(val == NULL)
            fprintf(stderr, "NO elements in Queue: %s\n", __func__);

    } PTH(err, pthread_mutex_unlock(&(q->mtx)))

    return val;
}

static int cassiere_sveglia_clienti(queue_t *q, attesa_t stato) {
    int err;

    node_t *ptr = q->head;
    queue_elem_t *curr;

    while(ptr != NULL) {
        curr = (queue_elem_t *) ptr->elem;
        curr->stato_attesa = stato;
        PTHLIB(err, pthread_cond_signal(&(curr->cond_cl_q)))
    }

    return 0;
}
