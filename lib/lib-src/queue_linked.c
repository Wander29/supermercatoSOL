/*
 * CODA FIFO CONCORRENTE, unbounded, produttori-consumatori
 * politica:
*
 * Mutua esclusione sia fra consumatori che fra produttori
 */
#include <queue_linked.h>
#include "../lib-include/queue_linked.h"

queue_t *start_queue(void) {
    queue_t *Q = calloc(1, sizeof(queue_t));
    if(Q == NULL) {
        // fprintf(stderr, "CALLOC fallita: %s\n", __func__);
        return NULL;
    }
    Q->nelems = 0;
    Q->tail = NULL;
    Q->head = NULL;

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

int free_queue(queue_t *Q, enum deallocazione_t opt) {
    QUEUENULL(Q, -1)

    node_t  *curr = Q->head,
            *curr_prev;
    while(curr != NULL) {
        curr_prev = curr;
        curr = curr->next;
        if(opt == DYNAMIC_ELEMS)
            free(curr_prev->elem);
        free(curr_prev);
    }
    free(Q);

    return 0;
}

queue_position_t queue_get_position(queue_t *Q, const void *elem) {
    node_t *ptr = Q->head;
    int cnt = 0;
    queue_position_t tmp = {-1, NULL};

    while(ptr != NULL) {
        if(ptr->elem == elem) {
            tmp.pos = cnt;
            tmp.ptr_in_queue = ptr;
            return tmp;
        }
        cnt++;
        ptr = ptr->next;
    }

    return tmp;
}

int queue_remove_specific_elem(queue_t *Q, node_t *ptr) {
    if(ptr == NULL || Q == NULL)
        return -1;

    if(Q->head == ptr)
        Q->head = ptr->next;

    if(Q->tail == ptr)
        Q->tail = ptr->prec;

    if(ptr->next != NULL)
        ptr->next->prec = ptr->prec;

    if(ptr->prec != NULL)
        ptr->prec->next = ptr->next;

    free(ptr);
    Q->nelems--;

    return 0;
}

void print_queue(queue_t *Q) {
    node_t *ptr = Q->head;
    int cnt = 0;
    puts("CODA");
    while(ptr != NULL) {
        printf("[%d] %p\n", cnt++, ptr->elem);
        ptr = ptr->next;
    }
}