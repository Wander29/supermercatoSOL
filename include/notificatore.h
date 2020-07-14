#ifndef LUDOVICO_VENTURI_NOTIFICATORE_H
#define LUDOVICO_VENTURI_NOTIFICATORE_H

#include <myutils.h>
#include <protocollo.h>
#include <mytypes.h>
#include <mypthread.h>
#include <supermercato.h>
#include <queue_linked.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "../include/mytypes.h"
#include "../lib/lib-include/mypthread.h"
#include "../lib/lib-include/myutils.h"
#include "../include/protocollo.h"
#include "../include/supermercato.h"

void *notificatore(void *arg);

#endif //LUDOVICO_VENTURI_NOTIFICATORE_H
