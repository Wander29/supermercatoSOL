#ifndef PROGETTO_PROTOCOLLO_H
#define PROGETTO_PROTOCOLLO_H

#include <pthread.h>
#include <mypthread.h>
#include <mysocket.h>

#define SOCKET_SERVER_NAME "pleasekillmeatexit"
#define PATH_TO_SUPERMARKET "./supermercato"
#define MAX_BACKLOG 2

typedef enum stato_supermercato {
    APERTO = 0,
    CHIUSURA_IMMEDIATA,
    CHIUSURA_SOFT
} stato_sm_t;

/** var. globali */
extern stato_sm_t stato_supermercato;
extern pthread_mutex_t mtx_stato_supermercato;

void set_stato_supermercato(const stato_sm_t x);
stato_sm_t get_stato_supermercato();


/**************************************************************************************
 * PROTOCOLLO COMUNICAZIONE supermercato - direttore
 *
 * possono scrivere al direttore entità differenti del supermercato:
 *  - clienti   nel caso in cui vogliano uscire senza aver fatto acquisti
 *  - cassieri  regolarmente per informare il direttore del numero di clienti in coda
 *  - manager   per informarlo dell'imminente chiusura del supermercato
 *              e per inviare il PID ad inizio connessione
 *
 *  il direttore scrive al supermercato per:
 *  - aprire o chiudere una cassa
 *  - concede permesso di uscire al cliente
 *
 * Ogni comunicazione avviene inviando in ordine:
 * 1) tipo di messaggio (elencati sotto)
 * 2) (se necessario) [parametro del messaggio (tutti di tipo intero)]
 **************************************************************************************/
typedef enum msg_code {
/*  1) tipo messaggio           |   2) tipo del parametro        */
    MANAGER_PID = 0,                /* pid_t */
    MANAGER_IN_CHIUSURA,            /* // */
    CASSIERE_NUM_CLIENTI = 100,     /* int */
    CLIENTE_SENZA_ACQUISTI = 200,   /* // */
    DIRETTORE_APERTURA_CASSA = 300, /* int */
    DIRETTORE_CHIUSURA_CASSA,       /* int */
    DIRETTORE_PERMESSO_USCITA       /* // */
} msg_code_t;

#endif