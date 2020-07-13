#ifndef LUDOVICO_VENTURI_MYTYPES_H
#define LUDOVICO_VENTURI_MYTYPES_H

#include <myutils.h>
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include "../lib/lib-include/queue_linked.h"
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <queue_linked.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/protocollo.h"

/************************************
 * PIPE: PROTOCOLLO comunicazione
 ************************************/
typedef enum pipe_msg_code {
    CLIENTE_RICHIESTA_PERMESSO = 0,
    CLIENTE_ENTRATA,
    CLIENTE_USCITA,
    SIG_RICEVUTO = 900
} pipe_msg_code_t;

/************************************
 * Argomento ai thread Cassieri
 ************************************/
typedef enum stato_cassa {
    CHIUSA = 0,
    APERTA
} stato_cassa_t;

typedef struct cassa_specific {
    pthread_cond_t cond_queue;
    pthread_cond_t cond_notif;
    pthread_mutex_t mtx_cassa;
    queue_t *q;
    stato_cassa_t stato;
    int index;
} cassa_specific_t;

typedef struct cassa_com_arg {
    pool_set_t *pool_set;
    int tempo_prodotto;
    int A;                      /* ampiezza intervallo di comunicazione col direttore */
} cassa_com_arg_t;

typedef struct cassa_arg {
    /* comune a tutte le casse */
    cassa_com_arg_t *shared;

    /* specifico per ogni cassa */
    cassa_specific_t *cassa_set;
} cassa_arg_t;

/************************************
 * Cliente in coda: elemento da salvare
 ************************************/
typedef enum attesa {
    IN_ATTESA = 0,
    SERVIZIO_IN_CORSO,
    SERVITO,
    CASSA_IN_CHIUSURA,
    SM_IN_CHIUSURA
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
    cassa_specific_t *casse;
    int numero_casse;
    int T;
    int P;
    int S;
    int current_last_id_cl;
    pthread_cond_t cond_id_cl;
    pthread_mutex_t mtx_id_cl;
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

/*
 * Gestione della cassa (forse) più conveniente (non OTTIMA)
 */
typedef struct min_queue {
    cassa_specific_t *ptr_q;
    int num;
} min_queue_t;

#endif //LUDOVICO_VENTURI_MYTYPES_H
