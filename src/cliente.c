#include "../include/cliente.h"

static void *cliente(void *arg) {
    cliente_arg_t *C = (cliente_arg_t *) arg;
    pool_set_t *P = C->pool_set;

#ifdef DEBUG_CLIENTE
    printf("[CLIENTE %d] sono nato!\n", C->index);
#endif

    int p = 0,
    // t,
    err;

    PTH(err, pthread_mutex_init(&(C->mtx), NULL))
    PTH(err, pthread_cond_init(&(C->cond), NULL))

    pipe_msg_code_t type_msg;

    // while(get_stato_supermercato() == APERTO) {
    PTH(err, pthread_mutex_lock(&(P->mtx)))
    while(P->jobs == 0) {
        PTH(err, pthread_cond_wait(&(P->cond), &(P->mtx)))
        /*
         * viene svegliato: si deve controllare se si deve terminare;
         * lascia la mutex per non avere due risorse lockate
         */
        PTH(err, pthread_mutex_unlock(&(P->mtx)))
        if(get_stato_supermercato() != APERTO) {
            pthread_exit((void *) NULL);
        }
        PTH(err, pthread_mutex_lock(&(P->mtx)))
    }
    P->jobs--;
    C->permesso_uscita = 0;
    PTH(err, pthread_mutex_unlock(&(P->mtx)))
    /*
     * fa acquisti
     */
    if(p == 0) {
        /*
         * vuole comunicare al direttore di voler uscire:
         *      scrive sulla pipe in mutua esclusione la sua richiesta, completa di indice cliente
         *      e si mette in attesa di una risposta
         */
#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] fateme uscìììììì!\n", C->index);
#endif
        type_msg = CLIENTE_RICHIESTA_PERMESSO;
        MENO1(pipe_write(&type_msg, &(C->index)))
        cliente_attendi_permesso_uscita(C);
    } else {
        /*
        * si mette in fila in una cassa
        */
        queue_elem_t elem;
        elem.num_prodotti = p;
        PTH(err, pthread_cond_init(&(elem.cond_cl_q), NULL))
        PTH(err, pthread_mutex_init(&(elem.mtx_cl_q), NULL))
        sleep(1);
        /* @TODO
         * prima di mettermi in coda dovrei controllare lo stato del supermercato
         */
        /*
         * devo vedere se la cassa è aperta!!!!!!!!!!
         */
        unsigned seedp;
        int x = rand_r(&seedp) % K;
        MENO1LIB(cliente_push(Q[x], &elem))
        while((err = cliente_attendi_servizio(&elem)) != 0) {
            if(err < 0) {
                fprintf(stderr, "ERRORE [CLIENTE] durante attesa del servizio\n");
            }
            /*
             * TODO migliorabile!
             */
            /*
             * se la cassa sta per chiudere
             */
            if(elem.stato_attesa == CASSA_IN_CHIUSURA) {
                int y = rand_r(&seedp) % K;

                MENO1LIB(cliente_push(Q[x], &elem))
            }
        }
    }
    //}

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))

    pthread_exit((void *)NULL);
}

static

static int cliente_attendi_servizio(queue_elem_t *e) {
    int err;
    PTHLIB(err, pthread_mutex_lock(&(e->mtx_cl_q))) {
        while(e->stato_attesa == IN_ATTESA) {
            PTHLIB(err, pthread_cond_wait(&(e->cond_cl_q), &(e->mtx_cl_q)))
            if(e->stato_attesa == SERVITO) {
                return 0;
            } else if(e->stato_attesa != IN_ATTESA)
                return 1;
        }
    } PTHLIB(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
}

static void cliente_attendi_permesso_uscita(cliente_arg_t *t) {
    int err;

    PTH(err, pthread_mutex_lock(&(t->mtx))) {
        while(t->permesso_uscita == 0)
            PTH(err, pthread_cond_wait(&(t->cond), &(t->mtx)))

#ifdef DEBUG_CLIENTE
        printf("[CLIENTE %d] Permesso di uscita ricevuto!\n", t->index);
#endif
    } PTH(err, pthread_mutex_unlock(&(t->mtx)))
}

static void consenti_uscita_cliente(cliente_arg_t *t) {
    int err;

    PTH(err, pthread_mutex_lock(&(t->mtx))) {
        t->permesso_uscita = 1;
        PTH(err, pthread_cond_signal(&(t->cond)))
    } PTH(err, pthread_mutex_unlock(&(t->mtx)))
}

int cliente_push(queue_t *q, queue_elem_t *new_elem) {
    int r;          // retval, per errori

    PTH(r, pthread_mutex_lock(&(q->mtx))) {
        if(insert_into_queue(q, (void *) new_elem) == -1) {
            PTH(r, pthread_mutex_unlock(&(q->mtx)))
            // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
            return -1;
        }
        PTH(r, pthread_mutex_unlock(&(q->mtx)))
        inc_num_clienti_in_coda();
        PTH(r, pthread_cond_signal(&(q->cond_read)))
    } PTH(r, pthread_mutex_unlock(&(q->mtx)))

    return 0;
}