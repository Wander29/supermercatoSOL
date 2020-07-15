#ifndef LUDOVICO_VENTURI_MYTYPES_H
#define LUDOVICO_VENTURI_MYTYPES_H

#include <pool.h>
#include <pthread.h>
#include <queue_linked.h>

#include "../lib/lib-include/queue_linked.h"

/*
#include <myutils.h>

#include <protocollo.h>
#include <stdlib.h>

#include <mypoll.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/protocollo.h"
*/

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
 * LOG
 ************************************/
typedef struct cliente_log {
    int id_cliente;
    int tempo_permanenza;
    int tempo_attesa;
    int tempo_acquisti;
    int num_cambi_cassa;
    int num_cambi_cassa_per_chiusura;
    int num_prodotti_acquistati;
} cliente_log_t;

typedef struct cassa_log {
    int id_cassa;
    int num_prodotti_elaborati;
    // int num_clienti_serviti;
    int num_chiusure;

    queue_t *aperture;
    queue_t *clienti_serviti;
} cassa_log_t;

typedef struct cliente_servito_log {
    int tempo_servizio;
    int id_cliente;
} cliente_servito_log_t;

typedef struct log_set {
    queue_t        **log_clienti;
    cassa_log_t     *log_casse;

    int K;
    int C;
} log_set_t;


/************************************
 * Argomento ai thread Cassieri
 ************************************/
typedef enum stato_cassa {
    CHIUSA = 0,
    APERTA
} stato_cassa_t;

typedef struct cassa_public {
    pthread_mutex_t mtx_cassa;
    pthread_cond_t cond_queue;
    queue_t *q;
    stato_cassa_t stato;
    int index;
} cassa_public_t;

typedef struct cassa_com_arg {
    pool_set_t *pool_set;
    int tempo_prodotto;
    int A;                      /* ampiezza intervallo di comunicazione col direttore */
} cassa_com_arg_t;

typedef struct cassa_arg {
    /* comune a tutte le casse */
    cassa_com_arg_t *shared;

    /* specifico per ogni cassa */
    cassa_public_t *cassa_set;
    pthread_cond_t cond_notif;
    cassa_log_t *log_cas;
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
    int id_cliente;
} queue_elem_t;

/****
 * CLIENTE
 */
typedef struct client_com_arg {
    pool_set_t *pool_set;
    cassa_public_t *casse;
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
    queue_elem_t *elem;
    queue_t      *log_cl;
} cliente_arg_t;

/*
 * Gestione della cassa (forse) più conveniente (non OTTIMA)
 */
typedef struct min_queue {
    cassa_public_t *ptr_q;
    int num;
} min_queue_t;

#endif //LUDOVICO_VENTURI_MYTYPES_H
