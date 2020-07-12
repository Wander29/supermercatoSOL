#include <parser_config.h>

int get_params_from_file(param_t *ptr) {
    ptr->C = 2;
    ptr->K = 2;
    ptr->E = 1;
    ptr->T = 200;
    ptr->P = 100;
    ptr->S = 20;
    ptr->L = 5;
    ptr->J = 2;
    ptr->A = 400;
    ptr->S1 = 1;
    ptr->S2 = 10;

    return 0;
}
