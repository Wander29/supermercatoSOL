#include "../include/cliente.h"

static void cliente_attendi_permesso_uscita(cliente_arg_t *t);
static int cliente_push(queue_t *q, queue_elem_t *new_elem);
static int cliente_attendi_servizio(queue_elem_t *e);

inline static int cliente_cassa_random(unsigned *seedptr, int k, cassa_specific_t **c) {
    int x;

    do {
        x = rand_r(seedptr) % k;
    } while(c[x]->stato != APERTA);

    return x;
}

void *cliente(void *arg) {
    cliente_arg_t *C = (cliente_arg_t *) arg;
    pool_set_t *P = C->shared->pool_set;
    cassa_specific_t **Casse = C->shared->casse;

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
#ifdef DEBUG_CLIENTE
    printf("[CLIENTE %d] comincio gli acquisti!\n", C->index);
#endif
    /*
     *  tempo per gli acquisti che varia in modo casuale da 10 a
        T>10 millisecondi, ed un numero di prodotti che acquisterà che varia da 0 a P>0
     */

    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {
        // TODO termina
    }
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
    }
    else {
        /*
        * si mette in fila in una cassa
        */
        queue_elem_t elem;
        elem.num_prodotti = p;
        PTH(err, pthread_cond_init(&(elem.cond_cl_q), NULL))
        PTH(err, pthread_mutex_init(&(elem.mtx_cl_q), NULL))

        unsigned seedp;

        int x = cliente_cassa_random(&seedp, C->shared->numero_casse, Casse);
        MENO1LIB(cliente_push(Casse[x]->q, &elem), ((void *)-1))

        while((err = cliente_attendi_servizio(&elem)) > 0) {
            /*
             * TODO migliorabile!
             */
            /*
             * se la cassa sta per chiudere
             */
            if(elem.stato_attesa == CASSA_IN_CHIUSURA) { // si accoda ad un'altra cassa random
                int y;
                do
                    y = cliente_cassa_random(&seedp, C->shared->numero_casse, Casse);
                while(x == y);

                MENO1LIB(cliente_push(Casse[x]->q, &elem), ((void *)-1))
            }
        }
        if(err < 0) {
            fprintf(stderr, "ERRORE [CLIENTE] durante attesa del servizio\n");
        }
    }
    //}

    PTH(err, pthread_mutex_destroy(&(C->mtx)))
    PTH(err, pthread_cond_destroy(&(C->cond)))

    pthread_exit((void *)NULL);
}

static int cliente_attendi_servizio(queue_elem_t *e) {
    int err, ret;
    PTHLIB(err, pthread_mutex_lock(&(e->mtx_cl_q))) {
        while(e->stato_attesa == IN_ATTESA) {
            PTHLIB(err, pthread_cond_wait(&(e->cond_cl_q), &(e->mtx_cl_q)))
            if(e->stato_attesa == SERVITO) {
                ret = 0;
                break;
            } else if(e->stato_attesa != IN_ATTESA) {
                ret = 1;
                break;
            }
        }
    } PTHLIB(err, pthread_mutex_unlock(&(e->mtx_cl_q)))
    return ret;
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