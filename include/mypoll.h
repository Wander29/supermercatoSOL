//
// Created by ludo on 03/07/20.
//

#ifndef PROGETTO_MYPOLL_H
#define PROGETTO_MYPOLL_H

#include <poll.h>
#include <stdlib.h>
#include <stdio.h>

#define START_SIZE  16

struct pollfd *start_pollfd();
int pollfd_add(struct pollfd **v, struct pollfd pfd, int *actual_len);
int pollfd_remove(struct pollfd *v, const int j, int *actual_len);
void print_pollfd(struct pollfd *v, int actual_len);

#endif //PROGETTO_MYPOLL_H
