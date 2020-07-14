#include <parser_writer.h>
#include "../include/parser_writer.h"

char *strtok_r(char *str, const char *delim, char **saveptr);

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
                        CHECK(S)
                        VINC(<10)   /* [S>=10] */
                        ptr->S = val;
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

int write_log(char *filepath, log_set_t log) {
    return 0;
}