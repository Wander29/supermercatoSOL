#ifndef PROGETTO_PARSER_CONFIG_H
#define PROGETTO_PARSER_CONFIG_H

typedef struct param {
    int K,              /*  casse totali */
        J,              /*  casse inizialmente aperte */
        C,              /*  numero clienti massimo nel supermercato */
        E,              /*  */
        T,              /*  */
        P,              /*  numero prodotti massimo acquistati da un cliente */
        S,              /*  */
        L,              /*  tempo gestione di un singolo prodtto di ogni cassiere */
        S1,             /* definisce il numero di casse con al più un cliente in coda
 *        es. S1=2: chiude una cassa (se possibile) se ci sono almeno 2 casse che hanno al più un cliente*/
        S2;             /* definisce il numero di clienti in coda in almeno una cassa
 *        es. S2=10: apre una cassa (se possibile) se c’è almeno una cassa con almeno 10 clienti in coda*/
} param_t;

int get_params_from_file(param_t *ptr);

#endif // PROGETTO_PARSER_CONFIG_H
