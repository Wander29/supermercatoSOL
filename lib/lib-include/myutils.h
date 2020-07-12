//
// Ludovico Venturi
//
#ifndef PROGETTO_MYUTILS_H
#define PROGETTO_MYUTILS_H

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>     // exit, EXIT_FAILURE
#include <unistd.h>     // ssize_t, read/write
#include <stdio.h>     //  perror
#include <time.h>     //  nanosleep
#include <errno.h>     //  nanosleep

#define msTOnsMULT  1000000

#define MENO1(v) \
    if( (v) == -1 ) { perror(#v); exit(EXIT_FAILURE); }

#define MENO1LIB(v, ret) \
    if( (v) == -1 ) { fprintf(stderr, "ERROR: %s\n", #v); return (ret); }

#define LT0(v) /* LT0 == less than 0 */         \
    if( (v) < 0 )       { perror(#v); exit(EXIT_FAILURE); }

#define NOTZERO(v)                              \
    if( (v) != 0 )      { perror(#v); exit(EXIT_FAILURE); }

#define EQNULL(v) \
    if( (v) == NULL )   { perror(#v); exit(EXIT_FAILURE); }

#define EQNULLLIB(v) \
    if( (v) == NULL )   { perror(#v); return -1; }

/* se la syscall è interrotta da un segnale (e l'handler ritorna) */
#define INTSYSC(call, cond_term, label_term)    \
    if( (call) == -1) {                         \
        if(errno == EINTR && (cond_term) != 0)  \
            goto label_term;                    \
        else {                                  \
            perror(#call);                      \
            exit(EXIT_FAILURE);                 \
        }                                       \
    }

ssize_t readn(int fd, void *ptr, size_t n);
/* @INPUT       fd              file descriptor da cui leggere
 *              ptr             puntatore al buffer su cui scrivere
 *              n               bytes da leggere
 * @EFFECTS     Reads "n" bytes from a descriptor
 * @RETURNS     n               successo, ha letto esattamente n bytes nel buffer
 *              [0,n)           numero bytes letti => lettura parziale o EOF
 *                              in caso di errori dopo la lettura di almento un byte ritorna
 *                              comunque i byte letti e non riporta l'errore
 *              -1              errore, non è stato letto nessun byte e non si è a fine FILE
 * !NOTES
 */

ssize_t writen(int fd, void *ptr, size_t n);
/* @INPUT       fd              file descriptor su cui scrivere
 *              ptr             puntatore al buffer da cui leggere ciò che si deve scrivere
 *              n               bytes da scrivere
 * @EFFECTS     Writes "n" bytes from a descriptor
 * @RETURNS     n               successo, ha scritto esattamente n bytes nel buffer
 *              [0,n)           numero bytes scritti, => scrittura parziale
 *                              in caso di errori dopo la scrittura di almento un byte ritorna
 *                              comunque i byte scritti e non riporta l'errore
 *              -1              errore, non è stato scritto nessun byte
 * !NOTES
 */

int millisleep(const int ms);

#endif //PROGETTO_MYUTILS_H
