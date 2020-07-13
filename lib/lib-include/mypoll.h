#ifndef LUDOVICO_VENTURI_MYPOLL_H
#define LUDOVICO_VENTURI_MYPOLL_H

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>

#define START_SIZE  16

struct pollfd *start_pollfd(void);
int pollfd_add(struct pollfd **v, struct pollfd pfd, int *actual_len);
int pollfd_remove(struct pollfd *v, const int j, int *actual_len);
void print_pollfd(struct pollfd *v, int actual_len);
void pollfd_destroy(struct pollfd *v);

#endif //LUDOVICO_VENTURI_MYPOLL_H
