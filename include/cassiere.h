#ifndef PROGETTO_CASSIERE_H
#define PROGETTO_CASSIERE_H

#include <myutils.h>
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <mypthread.h>
#include <mysocket.h>
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <concurrent_queue.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <cliente.h>
#include <supermercato.h>

#include "../include/pool.h"
#include "../include/protocollo.h"
#include "../include/concurrent_queue.h"
#include "../include/parser_config.h"
#include "../include/supermercato.h"
#include "../include/cliente.h"

#define CASSA_APERTA_CHECK(Q)                               \
    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {    \
        return (queue_elem_t *) 1;                          \
    }                                                       \
    if(get_stato_cassa((Q)) == CHIUSA) {                    \
        return (queue_elem_t *) 1;                          \
    }


#define MAX_TEMPO_FISSO 80
#define MIN_TEMPO_FISSO 20

/************************************
 * Argomento ai thread Cassieri
 ************************************/
typedef enum stato_cassa {
    CHIUSA = 0,     /* IF(cassa.stato == CHIUSA && get_stato_supermerato() == CHIUS.IMM) => cliente esci */
    APERTA
} stato_cassa_t;

typedef struct cassa_specific {
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    queue_t *q;
    stato_cassa_t stato;
    int index;
} cassa_specific_t;

typedef struct cassa_arg {
    /* comune a tutte le casse */
    pool_set_t *pool_set;
    int tempo_prodotto;

    /* specifico per ogni cassa */
    cassa_specific_t *cassa_set;
} cassa_arg_t;

/************************************
 * Cliente in coda: elemento da salvare
 ************************************/
typedef enum attesa {
    IN_ATTESA = 0,
    SERVITO,
    CASSA_IN_CHIUSURA,
    SUPERMERCATO_IN_CHIUSURA
} attesa_t;

typedef struct queue_elem {
    int num_prodotti;
    attesa_t stato_attesa;
    pthread_cond_t cond_cl_q;
    pthread_mutex_t mtx_cl_q;
} queue_elem_t;

static void *cassiere(void *arg);
static void *notificatore(void *arg);
static int cassiere_attesa_lavoro(pool_set_t *P);
static queue_elem_t *cassiere_pop_cliente(cassa_arg_t *C);
static stato_cassa_t get_stato_cassa(cassa_arg_t *cassa);
static void set_stato_cassa(cassa_specific_t *cassa, const stato_cassa_t s);


#endif //PROGETTO_CASSIERE_H
