//
// Created by ludo on 10/07/20.
//

#ifndef PROGETTO_CLIENTE_H
#define PROGETTO_CLIENTE_H

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
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include <supermercato.h>
#include <cassiere.h>

#include "../include/pool.h"
#include "../include/protocollo.h"
#include "../include/concurrent_queue.h"
#include "../include/parser_config.h"
#include "../include/supermercato.h"
#include "../include/cassiere.h"

typedef struct cliente_arg {
    /* comune a tutti i clienti */
    pool_set_t *pool_set;
    cassa_specific_t *casse;

    /* specifico per ogni cliente */
    int index;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
    int permesso_uscita;
} cliente_arg_t;

static void *cliente(void *arg);
static void cliente_attendi_permesso_uscita(cliente_arg_t *t);
static void consenti_uscita_cliente(cliente_arg_t *t);
static int cliente_push(queue_t *q, queue_elem_t *new_elem);
static int cliente_attendi_servizio(queue_elem_t *e);

#endif //PROGETTO_CLIENTE_H
