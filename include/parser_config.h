#ifndef PROGETTO_PARSER_CONFIG_H
#define PROGETTO_PARSER_CONFIG_H

typedef struct param {
    int K,               /*  casse totali */
        J,              /*  casse inizialmente aperte */
        C,              /*  numero clienti massimo nel supermercato */
        E,              /*  */
        T,              /*  */
        P,              /*  numero prodotti massimo acquistati da un cliente */
        S;               /*  */
} param_t;

int get_params_from_file(param_t *ptr);

#endif // PROGETTO_PARSER_CONFIG_H
