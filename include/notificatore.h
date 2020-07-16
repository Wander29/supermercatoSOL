#ifndef LUDOVICO_VENTURI_NOTIFICATORE_H
#define LUDOVICO_VENTURI_NOTIFICATORE_H

#include <myutils.h>
#include <mytypes.h>
#include <protocollo.h>
#include <mypthread.h>
#include <supermercato.h>
#include <queue_linked.h>

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#ifndef STABILIZATION_TIME
    #define STABILIZATION_TIME 300
#endif

void *notificatore(void *arg);

#endif //LUDOVICO_VENTURI_NOTIFICATORE_H
