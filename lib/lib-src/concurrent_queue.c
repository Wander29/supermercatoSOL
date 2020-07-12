/*
 * CODA FIFO CONCORRENTE, unbounded, produttori-consumatori
 * politica:
*
 * Mutua esclusione sia fra consumatori che fra produttori
 */
#include <concurrent_queue.h>
#include "../lib-include/concurrent_queue.h"

int start_queue(queue_t **Q) {
    *Q = calloc(1, sizeof(queue_t));
    if(*Q == NULL) {
        // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
        return -1;
    }
    (*Q)->nelems = 0;
    (*Q)->tail = NULL;
    (*Q)->head = NULL;
    MENO1( pthread_mutex_init (&((*Q)->mtx_queue), NULL))
    MENO1( pthread_cond_init  (&((*Q)->cond_read), NULL))

    return 0;
}

queue_t *start_queue2(void) {
    queue_t *Q = calloc(1, sizeof(queue_t));
    if(Q == NULL) {
        // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
        return NULL;
    }
    Q->nelems = 0;
    Q->tail = NULL;
    Q->head = NULL;
    MENO1( pthread_mutex_init (&(Q->mtx_queue), NULL))
    MENO1( pthread_cond_init  (&(Q->cond_read), NULL))

    return Q;
}

/*
 * @INPUT       Q               coda != NULL
 *              new_elem        elemento da inserire nella coda, dato generico
 * @EFFECTS     inserisce new_elem nella coda Q
 * @RETURNS     0               successo
 *              -1              errore nell'allocazione dinamica di memoria
 */
int insert_into_queue(queue_t *Q, void *new_elem) {
    QUEUENULL(Q, -1)
    node_t *new_node = calloc(1, sizeof(node_t));
    if(new_node == NULL) {
        return -1;
    }
    new_node->elem = new_elem;
    new_node->next = NULL;
    new_node->prec  = Q->tail;

    if(Q->head == NULL){        // non ci sono nodi nella coda
        Q->head = (Q->tail = new_node);
    } else {                    // inserisco in fondo, FIFO
        Q->tail->next   = new_node;
        Q->tail = new_node;
    }
    Q->nelems++;
    return 0;
}

/*
 * @INPUT       Q               coda != NULL
 * @EFFECTS     ritorna l'elemento in testa alla coda, estraendolo dalla struttura node_t.
 *              Dealloca il nodo in testa e restituisce il valore che conteneva
 * @RETURNS     val             valore del nodo in testa
 *              NULL            coda vuota
 */
void *get_from_queue(queue_t *Q) {
    QUEUENULL(Q, NULL)
    if(Q->nelems == 0) {
        return NULL;
    }
    node_t *node = Q->head;
    if(Q->nelems == 1) { // head=tail
        Q->head = (Q->tail = NULL);
    } else {
        Q->head = Q->head->next;
        Q->head->prec = NULL;
    }

    void *val = node->elem;
    free(node);
    Q->nelems--;
    return val;
}

/*
 * lock(mutex)
 * while(queue.isEmpty()
 *      wait(cond, mutex)
 * queue.get()
 * IF errori
 *      riporta errore
 * unlock(mutex)
 */
void *getFIFO(queue_t *Q) {
    QUEUENULL(Q, NULL)
    int r;          // retval, per errori
    void *val;

    PTH(r, pthread_mutex_lock(&(Q->mtx_queue)))

    while(Q->nelems == 0)
       PTH(r, pthread_cond_wait(&(Q->cond_read), &(Q->mtx_queue)))

    if( (val = get_from_queue(Q)) == NULL) {
        // fprintf(stderr, "NO elements in Queue: %s\n", __func__);
    }

    PTH(r, pthread_mutex_unlock(&(Q->mtx_queue)))

    return val;
}

/*
 *      lock(mutex)
        queue.insert()
        IF(error during writing)
            unlock(mutex)
            return err
        signal(cond)
        unlock(mutex)
 */
int insertFIFO(queue_t *Q, void *new_elem) {
    QUEUENULL(Q, -1)
    int r;          // retval, per errori

    PTH(r, pthread_mutex_lock(&(Q->mtx_queue)))

    if(insert_into_queue(Q, new_elem) == -1) {
        PTH(r, pthread_mutex_unlock(&(Q->mtx_queue)))
        // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
        return -1;
    }
    PTH(r, pthread_cond_signal(&(Q->cond_read)))
    PTH(r, pthread_mutex_unlock(&(Q->mtx_queue)))

    return 0;
}

int free_queue(queue_t *Q, enum deallocazione_t opt) {
    QUEUENULL(Q, -1)
    int r;
    PTH(r, pthread_mutex_lock(&(Q->mtx_queue)))
    node_t  *curr = Q->head,
            *curr_prev;
    while(curr != NULL) {
        curr_prev = curr;
        curr = curr->next;
        if(opt == DYNAMIC_ELEMS)
            free(curr_prev->elem);
        free(curr_prev);
    }
    PTHLIB(r, pthread_cond_destroy(&(Q->cond_read)))
    PTHLIB(r, pthread_mutex_unlock(&(Q->mtx_queue)))
    PTHLIB(r, pthread_mutex_destroy(&(Q->mtx_queue)))
    free(Q);

    return 0;
}

int queue_get_len(queue_t *Q) {
    int err,
        len;

    PTHLIB(err, pthread_mutex_lock(&(Q->mtx_queue))) {
        len = Q->nelems;
    } PTH(err, pthread_mutex_unlock(&(Q->mtx_queue)))

    return len;
}