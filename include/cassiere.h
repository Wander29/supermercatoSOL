#ifndef LUDOVICO_VENTURI_CASSIERE_H
#define LUDOVICO_VENTURI_CASSIERE_H

#include <myutils.h>
#include "../include/mytypes.h"
#include <protocollo.h>
#include <stdio.h>

#include <stdlib.h>
#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include "../lib/lib-include/myutils.h"
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <queue_linked.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <cliente.h>

#include <supermercato.h>
#include "../include/protocollo.h"
#include "../include/supermercato.h"
#include "../include/cliente.h"

typedef struct ret_pop {
    queue_elem_t *ptr;
    attesa_t stato;
} ret_pop_t;

#define MAX_TEMPO_FISSO 80
#define MIN_TEMPO_FISSO 20

void *          cassiere(void *arg);
int             set_stato_cassa(cassa_specific_t *cassa, const stato_cassa_t s);


#endif //LUDOVICO_VENTURI_CASSIERE_H
