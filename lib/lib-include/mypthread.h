#ifndef PROGETTO_MYPTHREAD_H
#define PROGETTO_MYPTHREAD_H

#include <stdio.h>     //  perror
#include <errno.h>     //  errno

#define PTH(r, pth_call)			                    \
    if ( ((r) = (pth_call)) != 0) {						\
        errno = r;									    \
        perror(#pth_call);								\
        pthread_exit(&r);								\
    }

#define PTHLIB(r, pth_call)			                    \
    if ( ((r) = (pth_call)) != 0) {						\
        errno = r;									    \
        perror(#pth_call);								\
        return -1;								\
    }

#define PTHJOIN(status, str)                            \
    if((status) == PTHREAD_CANCELED)                    \
        printf("Thread %s cancellato\n", str);          \
    else if((status) != (void *) 0) {                   \
        perror("Exit value on pthread_join");           \
        exit(EXIT_FAILURE);                             \
    }

#endif //PROGETTO_MYPTHREAD_H
