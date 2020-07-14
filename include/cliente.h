#ifndef LUDOVICO_VENTURI_CLIENTE_H
#define LUDOVICO_VENTURI_CLIENTE_H

#include <myutils.h>
#include "../include/mytypes.h"
#include <protocollo.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include <mypoll.h>
#include <pool.h>
#include <queue_linked.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <supermercato.h>

#include <cassiere.h>
#include "../include/protocollo.h"

#include "../include/supermercato.h"
#include "../include/cassiere.h"
#include "../lib/lib-include/queue_linked.h"
#include <parser_writer.h>

#define PERC_CHQUEUE    0.75
#define POS_MIN_CHQUEUE     1
#define MIN_TEMPO_ACQUISTI 10

void *      cliente(void *arg);
int         set_permesso_uscita(cliente_arg_t *c, const int new_perm);
int         set_stato_attesa(queue_elem_t *e, const attesa_t new_stato);

#endif //LUDOVICO_VENTURI_CLIENTE_H
