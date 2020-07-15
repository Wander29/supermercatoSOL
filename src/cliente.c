#include "../include/cliente.h"

static int              cliente_attesa_lavoro(pool_set_t *P);
static int              cliente_attendi_permesso_uscita(cliente_arg_t *t);
static stato_cassa_t    cliente_push(cassa_public_t *C, queue_elem_t *new_elem);
static attesa_t         cliente_attendi_servizio(cassa_public_t *C, queue_elem_t *e, int timeout_ms, int *cnt_cambi);
static int              get_and_inc_last_id_cl(client_com_arg_t *com);

void *cliente(void *arg) {
    int err;
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
    cassa_public_t *Casse = C->shared->casse;

    queue_elem_t *elem = C->elem;

    pipe_msg_code_t type_msg;
    unsigned seedp = C->index + (unsigned) time(NULL);
    /* var di supporto per l'inserimeno in cassa */
    attesa_t stato_att;
    int x;

    /* Informazioni per Log */
    queue_t *log_queue = C->log_cl;
    cliente_log_t *log_cl;
    struct timeval  entrata_supermercato,
                    inizio_attesa,
                    uscita,
                    result;
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
        pipe_write(&type_msg, 0);

        /* reinizializzazione valori */
        log_cl = NULL; /* vorrei evitare qualsiasi ottimizzazione del compilatore */
        log_cl = calloc(1, sizeof(cliente_log_t));

        log_cl->num_cambi_cassa                 = -1;
        log_cl->num_cambi_cassa_per_chiusura    = -1;

        running = 1;
        NOTZERO(set_permesso_uscita(C, 0))
        NOTZERO(set_stato_attesa(elem, IN_ATTESA))

        /* reinizializzo struttura per calcolare i tempi */
        memset(&entrata_supermercato, 0, sizeof(struct timeval));
        memset(&inizio_attesa, 0, sizeof(struct timeval));
        memset(&uscita, 0, sizeof(struct timeval));
        memset(&result, 0, sizeof(struct timeval));

        /* si prende un nuovo id_cliente in mutua esclusione */
        log_cl->id_cliente = get_and_inc_last_id_cl(C->shared);
        if(log_cl->id_cliente == -1) {
            fprintf(stderr, "ERRORE: get_and_inc_last_id_cl\n");
            return (void *) -1;
        }
        /*
         * inizializzazione valori casuali
         */
        log_cl->num_prodotti_acquistati     = rand_r(&seedp) % (C->shared->P + 1);
        log_cl->tempo_acquisti              = (rand_r(&seedp) % (C->shared->T - MIN_TEMPO_ACQUISTI + 1)) + MIN_TEMPO_ACQUISTI;
#ifdef DEBUG_RAND
        printf("[CLIENTE %d] p: [%d]\tt: [%d]\n",C->index, p, t);
#endif
    /**************************************
     * INIZIO ACQUISTI
     * - prendo il tempo:
     *      all'entrata nel supermercato
     *      all'entrata in coda
     *      all'uscita dalla coda
     **************************************/
        gettimeofday(&entrata_supermercato, NULL);
        MENO1(millisleep(log_cl->tempo_acquisti))

        if (get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
#ifdef DEBUG_TERM
            printf("[CLIENTE %d] terminato per chiusura supermercato, dopo acquisti!\n", C->index);
#endif
            free(log_cl);
            goto terminazione_cliente;
        }
            if (log_cl->num_prodotti_acquistati == 0) {
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
            MENO1(pipe_write(&type_msg, 1, &(C->index)))

            if( cliente_attendi_permesso_uscita(C) == 1 ) { /* termina */
#ifdef DEBUG_TERM
                printf("[CLIENTE %d] terminato per chiusura supermercato, dopo attesa richiesta uscita!\n", C->index);
#endif
                free(log_cl);
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
            elem->num_prodotti   = log_cl->num_prodotti_acquistati;
            elem->id_cliente     = log_cl->id_cliente;
            stato_cassa_t st;
            do {
                log_cl->num_cambi_cassa++;
                log_cl->num_cambi_cassa_per_chiusura++;
                for(;;) {
                    x = rand_r(&seedp) % (C->shared->numero_casse);

                    if( (st = cliente_push(Casse + x, elem)) == APERTA )
                        goto attendi_servizio;
                    else if(st == (stato_cassa_t) -1) {
                        fprintf(stderr, "ERRORE: cliente_push");
                        return ((void *) -1);
                    }

                    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
#ifdef DEBUG_TERM
                        printf("[CLIENTE %d] terminato per chiusura supermercato, durante scelta cassa!\n", C->index);
#endif
                        free(log_cl);
                        goto terminazione_cliente;
                    }
                }

                attendi_servizio:
                if(inizio_attesa.tv_usec == 0) {
                    gettimeofday(&inizio_attesa, NULL);
                }

                stato_att = cliente_attendi_servizio(Casse+x, elem, C->shared->S, &(log_cl->num_cambi_cassa));
                if (stato_att < 0){
                    fprintf(stderr, "ERRORE [CLIENTE %d]: cliente_attendi_servizio\n", C->index);
                    return (void *) -1;
                }
                else if(stato_att  == SM_IN_CHIUSURA) {
#ifdef DEBUG_TERM
                    printf("[CLIENTE %d] terminato per chiusura supermercato, ero in CODA!\n", C->index);
#endif
                    free(log_cl);
                    goto terminazione_cliente;
                }

            } while(stato_att == CASSA_IN_CHIUSURA);
            /* se esco sono stato servito! */
        }
    /*******************************************
     * Cliente SERVITO, sta per uscire
     * - prende i tempi
     * - comunica l'uscita dal supermercato (potrebbe rientrare come thread)
     * - se il supermercato sta chiudendo, il thread TERMINA
     ********************************************/
        gettimeofday(&uscita, NULL);
        /* viene modificato solo il sottraendo (inizio_attesa/entrata_supermercato) */
        if(log_cl->num_prodotti_acquistati != 0) {
            TIMEVAL_DIFF(&result, &uscita, &inizio_attesa)
            log_cl->tempo_attesa =  result.tv_sec * sTOmsMULT + result.tv_usec / msTOusMULT;/* in ms */
        } else {
            log_cl->tempo_attesa =  -1;
        }

        TIMEVAL_DIFF(&result, &uscita, &entrata_supermercato)
        log_cl->tempo_permanenza =  result.tv_sec * sTOmsMULT + result.tv_usec / msTOusMULT;/* in ms */
#ifdef DEBUG_TIMERS
        printf("[CLIENTE %d] attesa [%d]ms\tpermanenza [%d]ms\n", \
                log_cl->id_cliente, log_cl->tempo_attesa, log_cl->tempo_permanenza);
#endif
        running = 0;

    /* LOG: aggiungo il record alla LOG_squeue del thread cliente */
        MENO1LIB(insert_into_queue(log_queue, log_cl), (void *)-1)

        type_msg = CLIENTE_USCITA;
        pipe_write(&type_msg, 0);

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
        pipe_write(&type_msg, 0);
    }

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

static attesa_t cliente_attendi_servizio(cassa_public_t *C, queue_elem_t *e, int timeout_ms, int *cnt_cambi) {
    int err;
    struct timespec ts;
    MENO1(millitimespec(&ts, timeout_ms))

    PTH(err, pthread_mutex_lock(&(C->mtx_cassa))) {
        PTH(err, pthread_mutex_lock(&(e->mtx_cl_q))) {

            while(e->stato_attesa != SERVITO) {
                switch(e->stato_attesa) {
                    case SM_IN_CHIUSURA:
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                        PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                        return e->stato_attesa;

                    case CASSA_IN_CHIUSURA:
                        e->stato_attesa = IN_ATTESA;
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                        PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                        return CASSA_IN_CHIUSURA;

                    case SERVIZIO_IN_CORSO: /* potrà solo andare a SERVITO(il cliente corrente viene sempre servito) */
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                        PTH(err, pthread_cond_wait(&(e->cond_cl_q), &(C->mtx_cassa)))
                        PTH(err, pthread_mutex_lock(&(e->mtx_cl_q)))
                        break;

                    case IN_ATTESA:
                        PTH(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
                    /*******************************************************************
                     * CONTROLLO CAMBIO CASSA
                     * si può cambiare cassa un massimo di 10 volte (compresi i cambi per chiusura)
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
#ifdef CAMBIO_CASSA
                        stato_cassa_t ret_minq;
                        if(*(cnt_cambi) < 10) {
                            queue_position_t curr_pos = queue_get_position(C->q, (void *) e);
                            if(curr_pos.pos == -1) {
                                fprintf(stderr, "ERRORE: queue_get_position\n");
                                PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                return -1;
                            }

                            if(curr_pos.pos > POS_MIN_CHQUEUE) { // Se ho almeno 2 clienti davanti a me
                                min_queue_t minq = get_min_queue();

                                if (minq.num >= 0 && minq.num <= ((curr_pos.pos) * (PERC_CHQUEUE))) {
                                    if (queue_remove_specific_elem(C->q, curr_pos.ptr_in_queue) == -1) {
                                        fprintf(stderr, "ERRORE CLIENT: queue_remove_specific_elem\n");
                                        PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                        return (attesa_t) -1;
                                    }

                                    PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                    ret_minq = cliente_push(minq.ptr_q, e);
                                    switch (ret_minq) {
                                        case APERTA: /* mi sono spostato nella coda più conveniente, prendo il lock, per la wait */
                                            C = minq.ptr_q;
                                            (*cnt_cambi)++;
                                            PTH(err, pthread_mutex_lock(&(C->mtx_cassa)))
                                            break;

                                        case CHIUSA: /* mi rimetterò in fila in una delle casse aperte */
                                            return CASSA_IN_CHIUSURA;

                                        default: /* errore */
                                            fprintf(stderr, "ERRORE CLIENT: cliente_push, minq\n");
                                            return (attesa_t) -1;
                                    }
                                }
                            }

                            if( (err = pthread_cond_timedwait(&(e->cond_cl_q), &(C->mtx_cassa), &ts)) != 0 && err != ETIMEDOUT) {
                                fprintf(stderr, "ERRORE CLIENT: pthread_cond_timedwait\n");
                                PTH(err, pthread_mutex_unlock(&(C->mtx_cassa)))
                                return (attesa_t) -1;
                            }
                        } else { /* dopo 10 cambi aspetto normalmente */
                            PTH(err, pthread_cond_wait(&(e->cond_cl_q), &(C->mtx_cassa)))
                        }
#endif
#ifndef CAMBIO_CASSA
                        PTH(err, pthread_cond_wait(&(e->cond_cl_q), &(C->mtx_cassa)))
#endif
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

            PTHLIB(err, pthread_mutex_unlock(&(t->mtx)))
            if(get_stato_supermercato() == CHIUSURA_IMMEDIATA)
                return 1;
            PTHLIB(err, pthread_mutex_lock(&(t->mtx)))
        }
#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Permesso di uscita ricevuto!\n", t->index);
#endif
    } PTHLIB(err, pthread_mutex_unlock(&(t->mtx)))

    return 0;
}

static stato_cassa_t cliente_push(cassa_public_t *C, queue_elem_t *new_elem) {
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

void print_queue_clients(cassa_public_t *C) {
    queue_t *Q = C->q;
    node_t *ptr = Q->head;
    queue_elem_t *elem;

    int cnt = 0;
    printf("----- CASSA [%d]-----\n", C->index);
    while(ptr != NULL) {
        elem = ptr->elem;
        printf("[%3d] Cliente [%d]\n", cnt++, elem->id_cliente);
        ptr = ptr->next;
    }
    puts("");
}