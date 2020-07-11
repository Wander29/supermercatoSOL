//
// Created by ludo on 10/07/20.
//

#ifndef PROGETTO_CLIENTE_H
#define PROGETTO_CLIENTE_H

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

#include <supermercato.h>
#include <cassiere.h>

#include "../include/protocollo.h"
#include "../include/supermercato.h"
#include "../include/cassiere.h"

#define MIN_TEMPO_ACQUISTI 10

#define CHECK_CHIUSURA_IMM(stri)                                             \
                                           \
}

void *cliente(void *arg);
int get_permesso_uscita(cliente_arg_t *c);
int set_permesso_uscita(cliente_arg_t *c, const int new_perm);

#endif //PROGETTO_CLIENTE_H
