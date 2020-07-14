#include <parser_config.h>
#include "../lib-include/parser_config.h"

char *strtok_r(char *str, const char *delim, char **saveptr);

int get_params_from_file(param_t *ptr, char *filepath) {
    FILE *f;
    char buf[BUF_SIZE];

    if( (f = fopen(filepath, "r")) == NULL) {
        perror("fopen");
        return -1;
    }

    ptr->A = ptr->C = ptr->E = ptr->J = ptr->K = ptr->L = ptr->P = ptr->S = ptr->S1 = ptr->S2 = ptr->T = -1;

    char    *saveptr,
            *token_key,
            *token_val;
    while(fgets(buf, BUF_SIZE, f) != NULL) {
        printf("%s", buf);
        if(buf[0] == '#')
            continue;
        token_key = strtok_r(buf, "=", &saveptr);
        token_val = strtok_r(NULL, ";", &saveptr);

        switch(token_key[0]) {
            case 'A':
                ptr->A = strtol(token_val, NULL, 10);
                break;
            case 'C':
                ptr->C = strtol(token_val, NULL, 10);
                break;
            case 'E':
                ptr->E = strtol(token_val, NULL, 10);
                break;
            case 'J':
                ptr->J = strtol(token_val, NULL, 10);
                break;
            case 'K':
                ptr->K = strtol(token_val, NULL, 10);
                break;
            case 'L':
                ptr->L = strtol(token_val, NULL, 10);
                break;
            case 'P':
                ptr->P = strtol(token_val, NULL, 10);
                break;
            case 'S':
                switch(token_key[1]) {
                    case '1':
                        ptr->S1 = strtol(token_val, NULL, 10);
                        break;
                    case '2':
                        ptr->S2 = strtol(token_val, NULL, 10);
                        break;
                    default:
                        ptr->S = strtol(token_val, NULL, 10);
                        break;
                }
                break;
            case 'T':
                ptr->T = strtol(token_val, NULL, 10);
                break;
        }
    }

    printf("A [%d]\nC [%d]\nE [%d]\nJ [%d]\nK [%d]\nL [%d]\nP [%d]\nS [%d]\nS1 [%d]\nS2 [%d]\nT [%d]\n", \
            ptr->A, ptr->C, ptr->E, ptr->J, ptr->K, ptr->L, ptr->P, ptr->S, ptr->S1, ptr->S2, ptr->T);

    if(fclose(f) != 0) {
        perror("fclose");
        return -1;
    }

    return 0;
}
