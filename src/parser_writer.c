#include <parser_writer.h>
#include "../include/parser_writer.h"

int get_params_from_file(param_t *ptr, char *filepath) {
    FILE *f;
    char buf[BUF_SIZE];

    if( (f = fopen(filepath, "r")) == NULL) {
        perror("fopen");
        return -1;
    }
    /* inizializza valori, se poi qualcuno resterà -1 ritorna un errore */
    ptr->A = ptr->C = ptr->E = ptr->J = ptr->K = ptr->L = ptr->P = ptr->S = ptr->S1 = ptr->S2 = ptr->T = ptr->Z[0] = -1;

    char    *saveptr,
            *token_key,
            *token_val;

    while(fgets(buf, BUF_SIZE, f) != NULL) {
        if(buf[0] == '#')
            continue;
        token_key = strtok_r(buf, "=", &saveptr);
        token_val = strtok_r(NULL, ";", &saveptr);
        if(token_key[0] == 'Z') {
            strip_spaces(token_val);
            CHECK(Z[0])
            strncpy(ptr->Z, token_val, BUF_SIZE);
            continue;
        }
        /* altrimenti è un parametro numerico, lo converto */
        int val = strtol(token_val, NULL, 10);
        switch(token_key[0]) {
            case 'A':
                CHECK(A) /* SE un parametro è ripetuto -> errore */
                VINC(<0)  /* [A>=0] */
                ptr->A = val;
                break;
            case 'C':
                CHECK(C)
                VINC(<0)      /* [C>=0] */
                ptr->C = val;
                break;
            case 'E':
                CHECK(E)
                MISS(C)
                VINC(<0)          /*  [ IN [0, C] ] */
                VINC(> ptr->C)    /*  [ IN [0, C] ] */
                ptr->E = val;
                break;
            case 'J':
                CHECK(J)
                MISS(K)
                VINC(<0)      /* [ IN [0, K] ]*/
                VINC(> ptr->K)      /* [ IN [0, K] ]*/
                ptr->J = val;
                break;
            case 'K':
                CHECK(K)
                VINC(<0)    /* [>=0] */
                ptr->K = val;
                break;
            case 'L':
                CHECK(L)
                VINC(<0)    /* [>=0] */
                ptr->L = val;
                break;
            case 'P':
                CHECK(P)
                VINC(<0)    /* [>=0] */
                ptr->P = val;
                break;
            case 'S':
                switch(token_key[1]) {
                    case '1':
                        CHECK(S1)
                        VINC(<1)    /* [S1>=1] */
                        ptr->S1 = val;
                        break;
                    case '2':
                        CHECK(S2)
                        VINC(<1)     /* [S2>=1] */
                        ptr->S2 = val;
                        break;
                    default:
                        if(token_key[1] == '\0' || isblank(token_key[1])) {
                            CHECK(S)
                            VINC(<10)   /* [S>=10] */
                            ptr->S = val;
                        } else {
                            fprintf(stderr, "S[x] non valido\n");
                            return -1;
                        }
                        break;
                }
                break;
            case 'T':
                CHECK(T)
                VINC(<=10)      /* [T>10] */
                ptr->T = val;
                break;
        }
    }
    /* controllo se ho letto TUTTI i parametri */
    MISS(A)
    MISS(C)
    MISS(E)
    MISS(J)
    MISS(K)
    MISS(L)
    MISS(P)
    MISS(S)
    MISS(S1)
    MISS(S2)
    MISS(T)
    MISS(Z[0])
    if(strstr(ptr->Z, ".csv") == NULL) {
        fprintf(stderr, "%s: NOT a valid CSV file\n", ptr->Z);
        return -1;
    }
    /*
    printf("A [%d]\nC [%d]\nE [%d]\nJ [%d]\nK [%d]\nL [%d]\nP [%d]\nS [%d]\nS1 [%d]\nS2 [%d]\nT [%d]\nZ [%s]\n\n", \
            ptr->A, ptr->C, ptr->E, ptr->J, ptr->K, ptr->L, ptr->P, ptr->S, ptr->S1, ptr->S2, ptr->T, ptr->Z);

     */
    if(fclose(f) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}

static int cmp_log_clienti(const void *p1, const void *p2) {
    cliente_log_t *c1 = *(cliente_log_t **) p1;
    cliente_log_t *c2 = *(cliente_log_t **) p2;

    return ((c1->id_cliente) - (c2->id_cliente));
}

int write_log(char *filepath, log_set_t *log) {
    FILE *f;
    if( (f = fopen(filepath, "w")) == NULL) {
        perror("fopen");
        return -1;
    }
    int i, err, j;
    /*******************************************************************
     * SCRITTURA LOG su file
     * - raccoglie i dati dei LOG dalle varie strutture
     *      .clienti: C code unbounded
     *              ordina i clienti per ID_CLIENTE
     *      .cassieri: array di K elementi di tipo cassa_log_t
     *              .cassa_log_t: contiene 2 code unbounded
     *
     * - somma il numero di clienti totali e il numero di prodotti venduti
     * - scrive sul file in formato csv, scrivendo END a fine di ogni categoria, e a fine LOG
     *******************************************************************/

    /* recupero struttura del LOG */

    /* Clienti */
    queue_t **clienti = log->log_clienti;
    int num_clienti_totali = 0;
    for(i = 0; i < log->C; i++)
        num_clienti_totali += clienti[i]->nelems;

    cliente_log_t **cli_v;
    if( (cli_v = calloc(num_clienti_totali, sizeof(cliente_log_t *))) == NULL) {
        fprintf(stderr, "ERRORE: calloc\n");
        return -1;
    }

    for(i = 0, j = 0; i < log->C; i++) {
        while(clienti[i]->nelems > 0)
            cli_v[j++] = (cliente_log_t *) get_from_queue(clienti[i]);

        LT0(free_queue(clienti[i], DYNAMIC_ELEMS), -1)
    }
    qsort( &cli_v[0], num_clienti_totali, sizeof(cliente_log_t *), cmp_log_clienti);

    /* Cassieri */
    int tot_prodotti_venduti = 0;
    cassa_log_t *casse = log->log_casse;
    for(i = 0; i < log->K; i++)
        tot_prodotti_venduti += casse[i].num_prodotti_elaborati;
    int *tempo_apertura;
    cliente_servito_log_t *servizio;

            /* inizio scrittura */

    /* SUPERMERCATO */
    LT0( (err = fprintf(f, "SUPERMERCATO,TOT_CLIENTI_SERVITI,TOT_PRODOTTI_VENDUTI\n")), err)
    LT0( (err = fprintf(f, "%d,%d\n", num_clienti_totali, tot_prodotti_venduti)), err)
    LT0( (err = fprintf(f, "END\n")), err)

    /* CASSIERI */
    LT0( (err = fprintf(f, "CASSIERI,ID_CASSA,PRODOTTI_ELABORATI,CLIENTI_SERVITI,NUM_CHIUSURE," \
                     "[LISTA]TEMPI_APERTURA{TEMPO_APERTURA},[LISTA]TEMPI_SERIVIZIO{TEMPO_SERVIZIO}\n")), err)

    for(i = 0; i < log->K; i++) {
        LT0( (err = fprintf(f, "%d,%d,%d,%d,LISTA,", \
            casse[i].id_cassa, casse[i].num_prodotti_elaborati, casse[i].clienti_serviti->nelems, casse[i].num_chiusure)), err)

        /* scritture LISTE (di aperture e di clienti serviti */
        while(casse[i].aperture->nelems > 0) {
            tempo_apertura = (int *) get_from_queue(casse[i].aperture);
            LT0( (err = fprintf(f, "%d,", *tempo_apertura)), err)
            free(tempo_apertura);
        }
        LT0( (err = fprintf(f, "END,LISTA,")), err)
        LT0(free_queue(casse[i].aperture, DYNAMIC_ELEMS), -1)

        while(casse[i].clienti_serviti->nelems > 0) {
            servizio = (cliente_servito_log_t *) get_from_queue(casse[i].clienti_serviti);
            LT0( (err = fprintf(f, "%d,", servizio->tempo_servizio)), err)
            free(servizio);
        }
        LT0( (err = fprintf(f, "END\n")), err)
        LT0(free_queue(casse[i].clienti_serviti, DYNAMIC_ELEMS), -1)

    }
    LT0( (err = fprintf(f, "END\n")), err)

    /* CLIENTI */
    LT0( (err = fprintf(f,    "CLIENTI,ID_CLIENTE,TEMPO_PERMANENZA,TEMPO_ATTESA," \
                                "NUM_CAMBI_CASSA,PRODOTTI_ACQUISTATI\n")), err)

    for(i = 0; i < num_clienti_totali; i++) {
        LT0( (err = fprintf(f, "%d,%.3f,%.3f,%d,%d\n", \
                    cli_v[i]->id_cliente, \
                    (cli_v[i]->tempo_permanenza / (float) msTOsMULT),
                    cli_v[i]->tempo_attesa < 0 ? -1 : (cli_v[i]->tempo_attesa / (float) msTOsMULT), \
                    cli_v[i]->num_cambi_cassa, \
                    cli_v[i]->num_prodotti_acquistati )), err)
        free(cli_v[i]);
    }
    LT0( (err = fprintf(f, "END\n")), err)
    free(cli_v);

    /* END LOG */
    LT0( (err = fprintf(f, "END\n")), err)

    if(fclose(f) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}