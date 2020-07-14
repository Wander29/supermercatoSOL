#ifndef LUDOVICO_VENTURI_PARSER_WRITER_H
#define LUDOVICO_VENTURI_PARSER_WRITER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/mytypes.h"

#define BUF_SIZE 256

typedef struct param {
    int K,              /*  casse totali */
        J,              /*  casse inizialmente aperte */
        C,              /*  numero clienti massimo nel supermercato */
        E,              /*  */
        T,              /*  */
        P,              /*  numero prodotti massimo acquistati da un cliente */
        S,              /*  intervallo di tempo in cui ogni cliente decide se cambiare coda*/
        L,              /*  tempo gestione di un singolo prodtto di ogni cassiere */
        A,              /*  intervallo di comunicazione col Direttore da parte dei cassieri */
        S1,             /* definisce il numero di casse con al più un cliente in coda
 *        es. S1=2: chiude una cassa (se possibile) se ci sono almeno 2 casse che hanno al più un cliente*/
        S2;             /* definisce il numero di clienti in coda in almeno una cassa
 *        es. S2=10: apre una cassa (se possibile) se c’è almeno una cassa con almeno 10 clienti in coda*/
} param_t;

int get_params_from_file(param_t *ptr, char *filepath);

int write_log(char *filepath, log_set_t log);

#endif // LUDOVICO_VENTURI_PARSER_WRITER_H