#ifndef PROGETTO_SUPERMERCATO_H
#define PROGETTO_SUPERMERCATO_H

#include <myutils.h>
#include "../include/types.h"
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
#include <cassiere.h>
#include <cliente.h>

#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/protocollo.h"
#include "../lib/lib-include/pool.h"
#include "../lib/lib-include/concurrent_queue.h"
#include "../lib/lib-include/parser_config.h"

/** Var. GLOBALI */
extern int dfd;                 /* file descriptor del socket col direttore */
extern int pipefd_sm[2];        /* fd per la pipe, su pipefd_sm[0] lettura, su pipefd_sm[1] scrittura  */

/** LOCK */
extern pthread_mutex_t mtx_socket;
extern pthread_mutex_t mtx_pipe;

typedef enum pipe_msg_code {
    CLIENTE_RICHIESTA_PERMESSO = 0,
    CLIENTI_TERMINATI,
    SIG_CHIUSURA_IMM_RICEVUTO = 900
} pipe_msg_code_t;

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

inline static int socket_write(sock_msg_code_t *type, int *param){
    int err;

    PTH(err, pthread_mutex_lock(&mtx_socket))

    if(writen(dfd, type, sizeof(sock_msg_code_t)) == -1) {
        perror("writen");
        return -1;
    }
    if(param != NULL) {
        if(writen(dfd, param, sizeof(int)) == -1) {
            perror("writen");
            return -1;
        }
#ifdef DEBUG_SOCKET
        printf("[SUPERMERCATO] HO scritto sul socket [%d, %d]\n", *type, *param);
#endif
    } else {
#ifdef DEBUG_SOCKET
        printf("[SUPERMERCATO] HO scritto sul socket [%d]\n", *type);
#endif
    }

    PTH(err, pthread_mutex_unlock(&mtx_socket))

    return 0;
}

#endif //PROGETTO_SUPERMERCATO_H
