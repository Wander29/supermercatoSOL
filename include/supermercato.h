#ifndef LUDOVICO_VENTURI_SUPERMERCATO_H
#define LUDOVICO_VENTURI_SUPERMERCATO_H

#include <myutils.h>
#include <mytypes.h>
#include <mypoll.h>
#include <protocollo.h>
#include <parser_writer.h>
#include <pool.h>
#include <queue_linked.h>
#include <cassiere.h>
#include <cliente.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include "../include/mytypes.h"
#include "../include/protocollo.h"
#include "../lib/lib-include/pool.h"
#include "../lib/lib-include/queue_linked.h"
#include "parser_writer.h"

#define PTHLIBMQ(err, pth_spin_call)        \
    if( (err = pth_spin_call) != 0 ) {      \
        tmp.ptr_q   = (cassa_public_t *) -1;       \
        tmp.num     = -1;                   \
        pthread_spin_unlock(&spin);          \
        return tmp;                         \
}

/*
 * Qui sono contenture le variabili condivise fra i thread del processo Supermercato:
 *      .manager
 *      .thread signal handler
 *      .cassieri (e notificatori)
 *      .clienti
 */

/** Var. GLOBALI */
extern int dfd;                 /* file descriptor del socket col direttore */
extern int pipefd_sm[2];        /* fd per la pipe, su pipefd_sm[0] lettura, su pipefd_sm[1] scrittura  */
extern min_queue_t min_queue;   /* conterrà il ptr alla coda con minor numero di clienti */

/** LOCK */
extern pthread_mutex_t mtx_socket;
extern pthread_mutex_t mtx_pipe;
extern pthread_spinlock_t spin;

/*******************************************************************************
 * MIN QUEUE, con spinlock
 * la min queue viene aggiornata con la seguente politica, NON OTTIMA
 *
 * POP, cassiere
 *      ad ogni rimozione controllo se la coda corrente diventa la più conveniente
 * PUSH, cliente
 *      ad ogni inserimento controlla se la coda in cui ho effettuato
*       l'inserimento è quella in min_queue, in tal caso NON controlla se ci sono
 *      code più convenienti => incremento solamente il numero di clienti
 *              della min_queue, per ragioni di efficienza
 * CHIUSURA CASSA
 *      se viene chiusa la cassa indicata da min_queue
 *          i valori in min_queue vengono resettati, non viene trovata la cassa migliore
 *          fra le rimanenti
 * APERTURA CASSA
 *      diventa subito la cassa migliore, senza controlli, non avendo clienti
 * CAMBIO CASSA di un cliente
 *      se un cliente cambia cassa di certo la cassa da cui proviene non era
 *      la min_queue, pertanto NON eseguo confronti al fine di aggiornare la min queue
 *****************************************************************************/
/* CAMBIO CASSA (cliente) */
inline static min_queue_t get_min_queue(void) {
    int err;
    min_queue_t tmp;

    PTHLIBMQ(err, pthread_spin_lock(&spin)) {
        tmp.ptr_q   = min_queue.ptr_q;
        tmp.num     = min_queue.num;
    } PTHLIBMQ(err, pthread_spin_unlock(&spin))

    return tmp;
}

/* POP */
inline static int testset_min_queue(cassa_public_t *q, const int num_to_test) {
    int err;

    PTHLIB(err, pthread_spin_lock(&spin)) {
        if(min_queue.num == -1 || min_queue.num > num_to_test) {
            min_queue.ptr_q = q;
            min_queue.num   = num_to_test;
#ifdef DEBUG_MINQ
            printf("[MINQ] cambiata, ora ha [%d] clienti!\n", min_queue.num);
#endif
        }
    } PTHLIB(err, pthread_spin_unlock(&spin))

    return 0;
}

/* APERTURA CASSA */
inline static int set_min_queue(cassa_public_t *q, const int num_to_test) {
    int err;

    PTHLIB(err, pthread_spin_lock(&spin)) {
        min_queue.ptr_q = q;
        min_queue.num   = num_to_test;
#ifdef DEBUG_MINQ
        printf("[MINQ] cambiata, ora ha [%d] clienti!\n", min_queue.num);
#endif
    } PTHLIB(err, pthread_spin_unlock(&spin))

    return 0;
}

/* CHIUSURA cassa */
inline static int is_min_queue_testreset(cassa_public_t *q) {
    int err;

    PTHLIB(err, pthread_spin_lock(&spin)) {
        if(min_queue.ptr_q == q) {
            min_queue.ptr_q = NULL;
            min_queue.num   = -1;
#ifdef DEBUG_MINQ
            printf("[MINQ] resettata, ora ha [%d] clienti!\n", min_queue.num);
#endif
        }
    } PTHLIB(err, pthread_spin_unlock(&spin))

    return 0;
}

/* PUSH */
inline static int is_min_queue_testinc(cassa_public_t *q) {
    int err;

    PTHLIB(err, pthread_spin_lock(&spin)) {
        if(min_queue.ptr_q == q) {
            min_queue.num++;
#ifdef DEBUG_MINQ
            printf("[MINQ] incrementata, ora ha [%d] clienti!\n", min_queue.num);
#endif
        }
    } PTHLIB(err, pthread_spin_unlock(&spin))

    return 0;
}

/* scrittura in mutua esclusione di 1 o + messaggi sulla pipe
 * fra Manager-Clienti-TSH(supermercato) seguendo il protocollo di comunicazone */
inline static int pipe_write(pipe_msg_code_t *type, int *param) {
    int err;

    PTH(err, pthread_mutex_lock(&mtx_pipe))

    if(writen(pipefd_sm[1], type, sizeof(pipe_msg_code_t)) == -1) {
        perror("writen");
        return -1;
    }
    if(param != NULL) {
        if(writen(pipefd_sm[1], param, sizeof(int)) == -1) {
            perror("writen");
            return -1;
        }
    }

    PTH(err, pthread_mutex_unlock(&mtx_pipe))
    return 0;
}

/* scrittura in mutua esclusione di 1 o + messaggi (a seconda del tipo di messaggio)
 * da parte dei thread del supermercato */
inline static int socket_write(sock_msg_code_t *type, int cnt, ...){
    int err;
    va_list l;
    va_start(l, cnt);

    PTH(err, pthread_mutex_lock(&mtx_socket))

    if(writen(dfd, type, sizeof(sock_msg_code_t)) == -1) {
        perror("writen");
        return -1;
    }
#ifdef DEBUG_SOCKET
    printf("[SUPERMERCATO] HO scritto sul socket [%d]\n", *type);
#endif
    while(cnt-- > 0) {
        int *tmp = va_arg(l, int *);
        if(writen(dfd, tmp, sizeof(int)) == -1) {
            perror("writen");
            return -1;
        }
#ifdef DEBUG_SOCKET
        printf("[SUPERMERCATO] HO scritto sul socket [%d]\n", *tmp);
#endif
    }
    PTH(err, pthread_mutex_unlock(&mtx_socket))

    va_end(l);
    return 0;
}

#endif //LUDOVICO_VENTURI_SUPERMERCATO_H
