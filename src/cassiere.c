#include "../include/cassiere.h"

static int      cassiere_attesa_lavoro(pool_set_t *P);
static int      cassiere_pop_cliente(cassa_public_t *C, ret_pop_t *val);
static int      cassiere_sveglia_clienti(cassa_public_t *C, attesa_t stato);

/**********************************************************************************************************
 * LOGICA DEL CASSIERE
 * - viene creato => aspetta che la cassa venga aperta
 * - la prima votla che viene aperta spawna un thread secondario notificatore
 * - attende che ci siano clienti
 * - se ci sono clienti in coda li serve
 *
 * - se è CHIUSA, il manager lo può svegliare per:
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
 **********************************************************************************************************/
void *cassiere(void *arg) {
    int err;

    /****************************************************************************
    * Cassiere _ THREAD
     * - casting degli argomenti
     * - inizializzaione
     *      cond, mutex
    ****************************************************************************/
    cassa_arg_t *Cassa_args = (cassa_arg_t *) arg;
    cassa_public_t *C = Cassa_args->cassa_set;
    cassa_com_arg_t *Com = Cassa_args->shared;
    pool_set_t *P = Com->pool_set;

    pthread_t tid_notificatore = -1;
    void *status_notificatore;
    int i = C->index;

    unsigned seedp = i + (unsigned) time(NULL);

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

#ifdef DEBUG_NOTIFY
        printf("[CASSA %d] APERTA!\n", i);
#endif
        /************************************************************************
         * Quando OTTIENE il lavoro
         *  - se è la prima apertura, avvia il thread notificatore associato
         *      else avvisa il notificatore che la cassa è di nuovo aperta
         *  - apre la cassa e si mette in attesa di clienti
         *************************************************************************/
        if(tid_notificatore == (pthread_t ) -1) {
            PTH(err, pthread_create(&tid_notificatore, NULL, notificatore, arg))
        } else {
            PTH(err, pthread_cond_signal(&(Cassa_args->cond_notif))) /* signal al notificatore */
        }

        /* setta la cassa APERTA e la setta come la più conveniente in cui aspettare */
        MENO1LIB(set_stato_cassa(C, APERTA), (void *) -1)
        MENO1LIB(set_min_queue(C, 0), (void *) -1)

        /********************************************************************************
         * GESTIONE CLIENTI(cassa APERTA)
         * - controlla che la cassa non sia stata chiusa || che il supermercato non
         *          stia chiudendo immediatamente
         * - se ci sono clienti, ne serve uno
         ********************************************************************************/
        for(;;) {
            if((cassiere_pop_cliente(C, &cli)) == -1) {
                if (SM_IN_CHIUSURA == cli.stato) {
#ifdef DEBUG_TERM
                    printf("[CASSA %d] esco, ero sulla COND_READ!\n", C->index);
#endif
                    MENO1LIB(cassiere_sveglia_clienti(C, SM_IN_CHIUSURA), (void *) -1)
                    goto terminazione_cassiere;
                }
                else if (CASSA_IN_CHIUSURA == cli.stato) {

                    /* se era la coda più conveniente resetto min_queue */
                    MENO1LIB(is_min_queue_testreset(C), (void *)-1)

                    MENO1LIB(cassiere_sveglia_clienti(C, CASSA_IN_CHIUSURA), (void *)-1)
                    break;
                }
                else {
                    // gestione errore ritorno dalla funzione
                    fprintf(stderr, "ERRORE: cassiere, pop_cliente!\n");
                    return (void *) -1;
                }
            }

            /**************************************************
             * Cliente estratto dalla coda
             * - lo servo:
             *      - calcolo il tempo di servizio
             *      - simulo il servizio con una nanosleep
             *      - sveglio il cliente
             **************************************************/
            tempo_servizio = tempo_fisso + cli.ptr->num_prodotti * Com->tempo_prodotto;
#ifdef DEBUG_RAND
            printf("[CASSA %d] sto per aspettare [%d]ms!\n", C->index, tempo_servizio);
#endif
            MENO1(millisleep(tempo_servizio))

            NOTZERO((set_stato_attesa(cli.ptr, SERVITO)))
            //set_stato_attesa_cassa(C, cli.ptr, SERVITO);
            PTH(err, pthread_cond_signal(&(cli.ptr->cond_cl_q)))
        }
    }

terminazione_cassiere:
    NOTZERO(set_stato_cassa(C, CHIUSA))
    PTH(err, pthread_cond_signal(&(Cassa_args->cond_notif))) /* sveglio eventualmente il notificatore */

    if(tid_notificatore != (pthread_t) -1) {
        PTH(err, pthread_join(tid_notificatore, &status_notificatore))
        PTHJOIN(status_notificatore, "Notificatore cassiere")
    }

#ifdef DEBUG_TERM
    printf("[CASSIERE %d] TERMINATO CORRETTAMENTE!\n", i);
#endif

    return (void *) 0;
}

int set_stato_cassa(cassa_public_t *cassa, const stato_cassa_t s) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(cassa->mtx_cassa))) {
        cassa->stato = s;
    } PTHLIB(err, pthread_mutex_unlock(&(cassa->mtx_cassa)))

    return 0;
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
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
                return 1;

            PTH(err, pthread_mutex_lock(&(P->mtx)))
        }
        P->jobs--;
    } PTH(err, pthread_mutex_unlock(&(P->mtx)))

    return 0;
}

static int cassiere_pop_cliente(cassa_public_t *C, ret_pop_t *val) {
    int nelems;
    
    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
        val->stato = SM_IN_CHIUSURA;
        return -1;
    }
    queue_t *q = C->q;
    int err;          // retval, per errori

    PTHLIB(err, pthread_mutex_lock(&(C->mtx_cassa))) {
        if(C->stato == CHIUSA) {
            PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))
            val->stato = CASSA_IN_CHIUSURA;
            return -1;
        }
        while(q->nelems == 0) {
            PTHLIB(err, pthread_cond_wait(&(C->cond_queue), &(C->mtx_cassa)))

            PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
                val->stato = SM_IN_CHIUSURA;
                return -1;
            }
            PTHLIB(err, pthread_mutex_lock(&(C->mtx_cassa)))
            if(C->stato == CHIUSA) {
                PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                val->stato = CASSA_IN_CHIUSURA;
                return -1;
            }
        }
        val->ptr = (queue_elem_t *) get_from_queue(q);
        if(val->ptr == NULL) {
            fprintf(stderr, "NO elements in Queue: %s\n", __func__);
            PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))
            return -1;
        }

        val->ptr->stato_attesa = SERVIZIO_IN_CORSO;
    nelems = q->nelems;
    } PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))
    /* ho fatto la POP, controllo se la mia coda ora è la piú conveniente */
    MENO1LIB( testset_min_queue(C, nelems), -1)

    return 0;
}

static int cassiere_sveglia_clienti(cassa_public_t *C, attesa_t stato) {
    int err;
    queue_elem_t *curr;
    queue_t *q = C->q;

    PTHLIB(err, pthread_mutex_lock(&(C->mtx_cassa))) {
#ifdef DEBUG
        printf("! Sto per avvertire che: %s\n", stato == CASSA_IN_CHIUSURA ? "CASSA_IN_CHIUSURA" : "SM_IN_CHIUSURA");
        print_queue_clients(C);
#endif
        while( (curr = (queue_elem_t *) get_from_queue(q)) != NULL ) {
            MENO1LIB(set_stato_attesa(curr, stato), -1)
            PTHLIB(err, pthread_cond_signal(&(curr->cond_cl_q)))
        }
    } PTHLIB(err, pthread_mutex_unlock(&(C->mtx_cassa)))

    return 0;
}