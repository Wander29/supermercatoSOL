#ifndef LUDOVICO_VENTURI_CONCURRENT_QUEUE_H
#define LUDOVICO_VENTURI_CONCURRENT_QUEUE_H

#include <myutils.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * Coda FIFO concorrente unbounded
 * Ogni accesso alla coda è implementato in modo da garantire la mutua esclusione
 */

#define QUEUENULL(q, retval)                                \
    if((q) == NULL) {                               \
        fprintf(stderr, "NO Queue: %s\n", __func__);\
        errno = EINVAL;                             \
        return retval;                                \
    }

typedef struct node_type {
    void *elem;
    struct node_type *next,
            *prec;
} node_t;

typedef struct queue_type {
    int nelems;                 /* numero elementi nella coda*/
    node_t  *head,
            *tail;
} queue_t;

typedef struct queue_position {
    int pos;
    node_t *ptr_in_queue;
} queue_position_t;

enum deallocazione_t {DYNAMIC_ELEMS=0, NO_DYNAMIC_ELEMS=1};

int insert_into_queue(queue_t *Q, void *new_elem);
void *get_from_queue(queue_t *Q);

queue_t *start_queue(void);
/* @INPUT
 * @EFFECTS     inizializza una coda FIFO concorrente, restituendone il puntatore
 * @RETURNS     ptr               successo
 *              NULL              se non riesce ad allocare memoria per la coda
 * !NOTES       dovrebbe essere chiamata solamente da un thread per coda, tipicamente dal main
 */

// void *getFIFO(queue_t *Q);
/* @INPUT       Q               coda
 * @EFFECTS     estra dalla coda il nodo in testa, ne ritorna il valore contenuto e
 *                  libera la memoria relativa al nodo. La testa della coda diventa
 *                  il nodo successivo a quello estratto
*               Se la coda è vuota si blocca
 * @RETURNS     ptr             valore del nodo in testa
 *              NULL            se la coda è vuota nonostante il wakeup || la coda non esiste
 * !NOTES
 */

// int insertFIFO(queue_t *Q, void *new_elem);
/* @INPUT       Q               coda
 *              new_elem        valore da inserire in coda
 * @EFFECTS     alloca dinamicamente un nodo contenente il valore new_elem
 *              e lo inserisce in fondo alla coda
 * @RETURNS     0               successo
 *              -1              errore nell'allocazione di memoria dinamica
 * !NOTES
 */

int free_queue(queue_t *Q, enum deallocazione_t opt);
/* @INPUT       Q               coda
 *              opt             DYNAMIC_ELEMS, NO_DYNAMIC_ELEMS
 *                              indicano se gli elementi nei nodi vanno o meno deallocati
 * @EFFECTS     libere la memoria riservata a tutta la struttura relativa alla coda.
 *              Gli elementi nei nodi vengono deallocati se DYNAMIC_ELEMS è settato
 * @RETURNS     //
 * !NOTES       dovrebbe essere chiamata solamente da un thread per coda, tipicamente dal main
 */

queue_position_t queue_get_position(queue_t *Q, const void *elem);
int queue_remove_specific_elem(queue_t *Q, node_t *ptr);
void print_queue(queue_t *Q);

#endif //LUDOVICO_VENTURI_CONCURRENT_QUEUE_H
