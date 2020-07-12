#ifndef PROGETTO_SUPERMERCATO_H
#define PROGETTO_SUPERMERCATO_H

#include <myutils.h>
#include "../include/mytypes.h"
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <concurrent_queue.h>
#include <cassiere.h>
#include <cliente.h>

#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/protocollo.h"
#include "../lib/lib-include/pool.h"
#include "../lib/lib-include/concurrent_queue.h"
#include "../lib/lib-include/parser_config.h"

#define PTHLIBMQ(err, pth_spin_call)        \
    if( (err = pth_spin_call) != 0 ) {      \
        tmp.ptr_q   = (queue_t *) -1;       \
        tmp.num     = -1;                   \
    }                                       \
    return tmp;

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
 *      la min_queue, e come per la PUSH non gestisco il nuovo inserimento
 *****************************************************************************/

inline static min_queue_t get_min_queue(void) {
    int err;
    min_queue_t tmp;

    PTHLIBMQ(err, pthread_spin_lock(&spin)) {
        tmp.ptr_q   = min_queue.ptr_q;
        tmp.num     = min_queue.num;
    } PTHLIBMQ(err, pthread_spin_unlock(&spin))

    return tmp;
}

inline static int testset_min_queue(queue_t *q, const int num_to_test) {
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

inline static int set_min_queue(queue_t *q, const int num_to_test) {
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

inline static int is_min_queue_testreset(queue_t *q) {
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

inline static int is_min_queue_testinc(queue_t *q) {
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

#endif //PROGETTO_SUPERMERCATO_H
