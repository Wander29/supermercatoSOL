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

#endif //PROGETTO_MYPTHREAD_H
