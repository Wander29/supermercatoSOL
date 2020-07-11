#ifndef PROGETTO_MYTYPES_H
#define PROGETTO_MYTYPES_H

#include <myutils.h>
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <concurrent_queue.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/protocollo.h"

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

/****
 * CLIENTE
 */
typedef struct client_com_arg {
    pool_set_t *pool_set;
    cassa_specific_t **casse;
    int numero_casse;
    int T;
    int P;
} client_com_arg_t;

typedef struct cliente_arg {
    /* comune a tutti i clienti */
    client_com_arg_t *shared;

    /* specifico per ogni cliente */
    int index;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    int permesso_uscita;
} cliente_arg_t;


#endif //PROGETTO_MYTYPES_H
