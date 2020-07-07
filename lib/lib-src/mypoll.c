#include <mypoll.h>

static unsigned size = START_SIZE;

struct pollfd *start_pollfd() {
    struct pollfd *v;
    v = calloc(START_SIZE, sizeof(struct pollfd));
    if(v == NULL) {
        perror("calloc");
        return NULL;
    }
    return v;
}

static void realloc_pollfd(struct pollfd **v) {
    if(*v == NULL) {
        *v = start_pollfd();
        return;
    }

    // printf("realloc: [old size] %d ", size);
    *v = realloc(*v, (size *= 2) * sizeof(struct pollfd));
    if(*v == NULL) {
        perror("calloc");
    }
    // printf("[new size] %d\n", size);
}

int pollfd_add(struct pollfd **v, struct pollfd pfd, int *actual_len) {
    if(*v == NULL || *actual_len < 0 || *actual_len > size)
        return -1;

    if(*actual_len == size) {
        realloc_pollfd(v);
        if(*v == NULL)
            return -1;
    }

    (*v)[*actual_len].fd        = pfd.fd;
    (*v)[*actual_len].events    = pfd.events;
    (*actual_len)++;

    return 0;
}

/*
 * rimuove l'elemento j'esimo dall'array spostando all'indietro tutti gli elementi da j+1 .. actual_len-1
 */
int pollfd_remove(struct pollfd *v, const int j, int *actual_len) {
    if(v == NULL || j < 0 || j >= size || *actual_len > size || *actual_len < 0)
        return -1;

    /*
     * se devo rimuovere l'ultimo elemento semplicemente decremento actual_len;
     * altrimenti sposto gli altri elemnti indietro
     */
    for(int i=j; i < (*actual_len - 1); i++) {
        v[i] = (struct pollfd) v[i+1];
    }

    (*actual_len)--;
    return 0;
}

void print_pollfd(struct pollfd *v, int actual_len) {
    printf("\nContenuto del pollfd:\n");
    for(int i=0; i<actual_len; i++)
        printf("[%d]: fd %d\n", i, v[i].fd);
}


