//
// Created by ludo on 10/07/20.
//

#ifndef PROGETTO_CLIENTE_H
#define PROGETTO_CLIENTE_H

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
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include <supermercato.h>
#include <cassiere.h>

#include "../include/protocollo.h"
#include "../include/supermercato.h"
#include "../include/cassiere.h"

void *cliente(void *arg);

#endif //PROGETTO_CLIENTE_H
