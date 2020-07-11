#ifndef PROGETTO_CASSIERE_H
#define PROGETTO_CASSIERE_H

#include <myutils.h>
#include "../include/mytypes.h"
#include <protocollo.h>
#include <stdio.h>

#include <stdlib.h>
#include <pthread.h>
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/mysocket.h"
#include <mypoll.h>
#include <parser_config.h>
#include <pool.h>
#include <concurrent_queue.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <cliente.h>

#include <supermercato.h>
#include "../include/protocollo.h"
#include "../include/supermercato.h"
#include "../include/cliente.h"

#define CASSA_APERTA_CHECK(Q)                               \
    if(get_stato_supermercato() == CHIUSURA_IMMEDIATA) {    \
        return (queue_elem_t *) 1;                          \
    }                                                       \
    if(get_stato_cassa((Q)) == CHIUSA) {                    \
        return (queue_elem_t *) 1;                          \
    }

#define MAX_TEMPO_FISSO 80
#define MIN_TEMPO_FISSO 20

void *cassiere(void *arg);
stato_cassa_t get_stato_cassa(cassa_specific_t *cassa);
void set_stato_cassa(cassa_specific_t *cassa, const stato_cassa_t s);


#endif //PROGETTO_CASSIERE_H
