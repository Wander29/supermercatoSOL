#ifndef PROGETTO_MYSOCKET_H
#define PROGETTO_MYSOCKET_H

#include <myutils.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>         // indirizzi AF_UNIX

#define UNIX_PATH_MAX 108

#define SOCKADDR_UN(addr, name) \
    do { memset( &(addr), '0', sizeof(addr)); (addr).sun_family = AF_UNIX; strncpy((addr).sun_path, (name), UNIX_PATH_MAX); } while(0);

#define SOCKETAF_UNIX(sfd) \
    MENO1((sfd) = socket(AF_UNIX, SOCK_STREAM, 0))


#endif //PROGETTO_MYSOCKET_H
