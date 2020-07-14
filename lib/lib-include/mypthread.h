#ifndef LUDOVICO_VENTURI_MYPTHREAD_H
#define LUDOVICO_VENTURI_MYPTHREAD_H

#include <stdio.h>     //  perror
#include <pthread.h>     //  errno
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
        return -1;								        \
    }

#define PTHLIBR(r, pth_call, ret)			            \
    if ( ((r) = (pth_call)) != 0) {						\
        errno = r;									    \
        perror(#pth_call);								\
        return (ret);								    \
    }

#define PTHJOIN(status, str)                            \
    if((status) == PTHREAD_CANCELED)                    \
        printf("Thread %s cancellato\n", str);          \
    else if((status) != (void *) 0) {                   \
        fprintf(stderr, "Exit value on pthread_join: %p\n", (void *)(status));           \
        exit(EXIT_FAILURE);                             \
    }

#endif //LUDOVICO_VENTURI_MYPTHREAD_H
