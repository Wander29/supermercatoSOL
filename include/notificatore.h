#ifndef LUDOVICO_VENTURI_NOTIFICATORE_H
#define LUDOVICO_VENTURI_NOTIFICATORE_H

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
#include <parser_writer.h>
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

void *notificatore(void *arg);

#endif //LUDOVICO_VENTURI_NOTIFICATORE_H
