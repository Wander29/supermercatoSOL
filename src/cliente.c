#include "../include/cliente.h"

static int              cliente_attesa_lavoro(pool_set_t *P);
static int              cliente_attendi_permesso_uscita(cliente_arg_t *t);
static stato_cassa_t    cliente_push(cassa_specific_t *C, queue_elem_t *new_elem);
static attesa_t         cliente_attendi_servizio(cassa_specific_t *C, queue_elem_t *e, int timeout_ms);
static int              get_and_inc_last_id_cl(client_com_arg_t *com);

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
     *      - se il supermercato é in chiusura (SOFT o IMMEDIATA)
     *              il cliente viene svegliato, e deve terminare
     * - entra nel supermercato
     *      avvisa il manager (il quale incrementerà il numero di clienti attivi nel supermercato)
     * - fa acquisti
     *      - numero prodotti IN [0, P]
     *      - tempo acquisti IN [10, T]ms
     * - se il supermercato viene chiuso
     *      - termina
     * - se non acquista nulla
     *      chiede il permesso per uscire
     *          una volta ricevuto,  torna ad ATTESA LAVORO
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

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    queue_elem_t elem;
    PTH(err, pthread_cond_init(&(elem.cond_cl_q), NULL))
    PTH(err, pthread_mutex_init(&(elem.mtx_cl_q), NULL))

    pipe_msg_code_t type_msg;
    unsigned seedp = C->index + (unsigned) time(NULL);
    /* var di supporto per l'inserimeno in cassa */
    attesa_t stato_att;
    int x;

    /* Informazioni per Log */
    int id_cliente;
    /*
    int num_cambi_cassa;
    struct timespec tempo_permanenza;
    struct timespec tempo_attesa;
     */
    // int clock_gettime(clockid_t clockid, struct timespec *tp);

    /************************************************
     * Vita del Cliente
     ************************************************/
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
        id_cliente = get_and_inc_last_id_cl(C->shared);
        if(id_cliente == -1) {
            fprintf(stderr, "ERRORE: get_and_inc_last_id_cl\n");
            return (void *) -1;
        }
        /*
         * inizializzazione valori casuali
         */
        // p = rand_r(&seedp) % (C->shared->P + 1);
        t = (rand_r(&seedp) % (C->shared->T - MIN_TEMPO_ACQUISTI + 1)) + MIN_TEMPO_ACQUISTI;
#ifdef DEBUG_RAND
        printf("[CLIENTE %d] p: [%d]\tt: [%d]\n",C->index, p, t);
#endif
        p = 0;
        /*
         * fa acquisti
         */
        MENO1(millisleep(t))
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

            if( (err = cliente_attendi_permesso_uscita(C)) == 1 ) { /* termina */
#ifdef DEBUG_TERM
                printf("[CLIENTE %d] terminato per chiusura supermercato, dopo attesa richiesta uscita!\n", C->index);
#endif
                goto terminazione_cliente;
            }
        } else {
            /*****************************************************
             * CLIENTE: pagamento in cassa, attesa del servizio
             * - sceglie la coda
             * - prova a pusharsi
             *      SE la cassa era chiusa, ne prova un'altra
             * - attendo il servizio
             *      SE vengo svegliato ma non sono servito, 2 casi
             *          SM_IN_CHIUSURA -> termino
             *          CASSA_IN_CHIUSURA -> ciclo!
             ****************************************************/
            elem.num_prodotti   = p;
            do {
                do {
                    x = rand_r(&seedp) % (C->shared->numero_casse);

                    err = (int) cliente_push(Casse + x, &elem);
                    if(err == -1) {
                        fprintf(stderr, "ERRORE: cliente_push");
                        return ((void *) -1);
                    }
                } while(err != (int) APERTA && get_stato_supermercato() != CHIUSURA_IMMEDIATA);

                stato_att = cliente_attendi_servizio(Casse+x, &elem, C->shared->S);
                if (stato_att < 0){
                    fprintf(stderr, "ERRORE [CLIENTE %d]: cliente_attendi_servizio\n", C->index);
                    return (void *) -1;
                }
                else if(stato_att  == SM_IN_CHIUSURA) {
#ifdef DEBUG_TERM
                    printf("[CLIENTE %d] terminato per chiusura supermercato, ero in CODA!\n", C->index);
#endif
                    goto terminazione_cliente;
                }

            } while(stato_att == CASSA_IN_CHIUSURA);
            /* se esco sono stato servito! */
        }
        running = 0;
        /*******************************************
         * Cliente SERVITO, sta per uscire
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

static attesa_t cliente_attendi_servizio(cassa_specific_t *C, queue_elem_t *e, int timeout_ms) {
    int err;
    struct timespec ts;
    MENO1(millitimespec(&ts, timeout_ms))

    PTH(err, pthread_mutex_lock(&(C->mtx_cassa))) {
        PTH(err, pthread_mutex_lock(&(e->mtx_cl_q))) {

            while(e->stato_attesa != SERVITO) {
                switch(e->stato_attesa) {
                    case SM_IN_CHIUSURA:
                    case CASSA_IN_CHIUSURA:
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                        PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                        return e->stato_attesa;

                    case SERVIZIO_IN_CORSO: /* potrà solo andare a SERVITO(il cliente corrente viene sempre servito) */
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                        PTH(err, pthread_cond_wait(&(e->cond_cl_q), &(C->mtx_cassa)))
                        PTH(err, pthread_mutex_lock(&(e->mtx_cl_q)))
                        break;

                    case IN_ATTESA:
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                    /*******************************************************************
                     * CONTROLLO CAMBIO CASSA
                     * Stato attuale (io==cliente):
                     *      Q[i]    cassa in cui mi trovo
                     *      e       elemento della coda che mi contiene
                     *
                     *      lock(Q[i]) -> la ho già dalla timedwait
                     *      x = posizione nella coda corrente (sono l'(x+1)-esimo in coda)
                     *      IF(la coda più conveniente ha almeno un 25% di clienti in meno in coda)
                     *         mi rimuovo da questa coda
                     *         provo a inserirmi nell'altra
                     *      else
                     *          unlock(Q[i])
                     *          ret e.stato
                     *******************************************************************/
#ifdef DIOG
                        queue_position_t curr_pos = queue_get_position(q, (void *) e);

                        if(curr_pos.pos > POS_MIN_CHQUEUE) { // Se ho almeno 2 clienti davanti a me
                            min_queue_t minq = get_min_queue();

                            if (minq.num <= (float) ((curr_pos.pos) * (PERC_CHQUEUE))) {
#ifdef DEBUG_CHQUEUE
                                //print_queue(q);
#endif
                                if (queue_remove_specific_elem(q, curr_pos.ptr_in_queue) == -1) {
                                    fprintf(stderr, "ERRORE CLIENT: queue_remove_specific_elem\n");
                                    PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                    return (attesa_t) -1;
                                }
                                PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                /* PUNTO CRITICO */
                                if ( (err = cliente_push(minq.ptr_q, e)) == CHIUSA) {
                                /* provo a reinserirmi nella coda di partenza, in coda */
                                    if ( (err = cliente_push(C, e)) == CHIUSA) {
                                        if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
                                            return SM_IN_CHIUSURA;
                                        else
                                            return CASSA_IN_CHIUSURA;
                                    } else if(err == APERTA) { /* SE mi sono rimesso in coda alla stessa cassa */
                                        PTH(err, pthread_mutex_lock(&(C->mtx_cassa)))
                                    } else {
                                        fprintf(stderr, "ERRORE CLIENT: cliente_push, current\n");
                                        return (attesa_t) -1;
                                    } /* chiusura ELSE: push in current queue */

                                } else if(err == APERTA) { /* SE mi sono messo in coda nella min_queue */
                                    C = minq.ptr_q;
                                    PTH(err, pthread_mutex_lock(&(C->mtx_cassa)))
                                } else {
                                    fprintf(stderr, "ERRORE CLIENT: cliente_push, minq\n");
                                    return (attesa_t) -1;
                                } /* chiusura ELSE: push in minq queue */
                            }
                        }
#endif

                        if( (err = pthread_cond_timedwait(&(e->cond_cl_q), &(C->mtx_cassa), &ts)) != 0 && err != ETIMEDOUT) {
                            fprintf(stderr, "ERRORE CLIENT: pthread_cond_timedwait\n");
                            PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                            return (attesa_t) -1;
                        }
                        PTH(err, pthread_mutex_lock(&(e->mtx_cl_q)))
                        break;

                    default: ;
                }
            }

        } PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
    } PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))

    return SERVITO;
}

static int cliente_attendi_permesso_uscita(cliente_arg_t *t) {
    int err;

    PTHLIB(err, pthread_mutex_lock(&(t->mtx))) {
        while(t->permesso_uscita == 0) {
            PTHLIB(err, pthread_cond_wait(&(t->cond), &(t->mtx)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
                return 1;
        }
#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Permesso di uscita ricevuto!\n", t->index);
#endif
    } PTHLIB(err, pthread_mutex_unlock(&(t->mtx)))

    return 0;
}

static stato_cassa_t cliente_push(cassa_specific_t *C, queue_elem_t *new_elem) {
    int r;          // retval, per errori
    queue_t *q = C->q;

    PTHLIB(r, pthread_mutex_lock(&(C->mtx_cassa))) {
        if(C->stato == CHIUSA) {
            PTHLIB(r, pthread_mutex_unlock(&(C->mtx_cassa)))
            return CHIUSA;
        }

        if(insert_into_queue(q, (void *) new_elem) == -1) {
            PTHLIB(r, pthread_mutex_unlock(&(C->mtx_cassa)))
            fprintf(stderr, "[CLIENTE] inserimento fallito: %s\n", __func__);
            return (stato_cassa_t) -1;
        }
    } PTHLIB(r, pthread_mutex_unlock(&(C->mtx_cassa)))


    PTHLIB(r, pthread_cond_signal(&(C->cond_queue)))

    MENO1LIB(is_min_queue_testinc(C), (stato_cassa_t) -1)
    return APERTA;
}

static int get_and_inc_last_id_cl(client_com_arg_t *com) {
    int err,
        ret;

    PTH(err, pthread_mutex_lock(&(com->mtx_id_cl))) {
        ret = ++(com->current_last_id_cl);
    } PTH(err, pthread_mutex_unlock(&(com->mtx_id_cl)))

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