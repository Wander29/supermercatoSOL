#ifndef LUDOVICO_VENTURI_PROTOCOLLO_H
#define LUDOVICO_VENTURI_PROTOCOLLO_H

#include <pthread.h>
#include <mypthread.h>

#include "../lib/lib-include/mypthread.h"

#define SOCKET_SERVER_NAME "pleasekillmeatexit.socket"
#define PATH_TO_SUPERMARKET "./bin/supermercato"
#define MAX_BACKLOG 2

/*
#define DEBUG_CLIENTE
#define DEBUG
#define DEBUG_RAND
#define DEBUG_WAIT
#define DEBUG_PIPE
#define DEBUG_MUTEX
#define DEBUG_SOCKET
#define DEBUG_CASSIERE
#define DEBUG_MANAGER
#define DEBUG_QUEUE
#define DEBUG_WAKE
#define DEBUG_MINQ
#define DEBUG_DIRETT
#define DEBUG_NOTIFY
#define DEBUG_CHANGEVERO
#define DEBUG_TIMERS
#define DEBUG_TERM
*/
#define CAMBIO_CASSA

typedef enum stato_supermercato {
    APERTO = 0,
    CHIUSURA_IMMEDIATA,
    CHIUSURA_SOFT
} stato_sm_t;

/** var. globali */
extern stato_sm_t stato_supermercato;
extern pthread_mutex_t mtx_stato_supermercato;

int set_stato_supermercato(const stato_sm_t x);
stato_sm_t get_stato_supermercato(void);

/**************************************************************************************
 * PROTOCOLLO COMUNICAZIONE supermercato - direttore
 *
 * possono scrivere al direttore entità differenti del supermercato:
 *  - clienti   nel caso in cui vogliano uscire senza aver fatto acquisti
 *  - cassieri  regolarmente per informare il direttore del numero di clienti in coda
 *  - manager   per informarlo dell'imminente chiusura del supermercato
 *
 *  il direttore scrive al supermercato per:
 *  - aprire o chiudere una cassa
 *  - concede permesso di uscire al cliente
 *
 * Ogni comunicazione avviene inviando in ordine:
 * 1) tipo di messaggio (elencati sotto)
 * 2) (se necessario) [parametro del messaggio (tutti di tipo intero)]
 **************************************************************************************/

typedef enum sock_msg_code {
/*  1) tipo messaggio           |   2) tipo del parametro        */
    MANAGER_IN_CHIUSURA     = 0,        /* // */
    CASSIERE_NUM_CLIENTI    = 100,      /* int: id cassa, int: numero clienti in coda */
    CLIENTE_SENZA_ACQUISTI  = 200,      /* int: id thread cliente */
    DIRETTORE_APERTURA_CASSA= 300,      /* // */
    DIRETTORE_CHIUSURA_CASSA,           /* int: id cassa */
    DIRETTORE_PERMESSO_USCITA           /* int: id thread cliente */
} sock_msg_code_t;

#endif