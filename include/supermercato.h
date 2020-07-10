#ifndef PROGETTO_SUPERMERCATO_H
#define PROGETTO_SUPERMERCATO_H

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
#include <cassiere.h>
#include <cliente.h>

#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include "../include/pool.h"
#include "../include/protocollo.h"
#include "../include/concurrent_queue.h"
#include "../include/parser_config.h"

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
