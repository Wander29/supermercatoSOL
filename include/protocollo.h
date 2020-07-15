#ifndef LUDOVICO_VENTURI_PROTOCOLLO_H
#define LUDOVICO_VENTURI_PROTOCOLLO_H

#define SOCKET_SERVER_NAME "pleasekillmeatexit.socket"
#define PATH_TO_SUPERMARKET "./bin/supermercato"
#define MAX_BACKLOG 2

/**************************************************************************************
 * PROTOCOLLO COMUNICAZIONE supermercato - direttore
 *
 * possono scrivere al direttore entità differenti del supermercato:
 *  - clienti   (attraverso il manager) nel caso in cui vogliano uscire senza aver fatto acquisti
 *  - cassieri  regolarmente per informare il direttore del numero di clienti in coda
 *  - manager   per informarlo dell'imminente chiusura del supermercato
 *
 *  il direttore scrive al supermercato per:
 *  - aprire o chiudere una cassa
 *  - concede permesso di uscire al cliente
 *
 * Ogni comunicazione avviene inviando in ordine:
 * 1) tipo di messaggio (elencati in sock_msg_code_t)
 * 2) (se necessario) [parametro del messaggio (tutti di tipo intero)]
 **************************************************************************************/

#endif