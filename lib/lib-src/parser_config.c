#include <parser_config.h>

int get_params_from_file(param_t *ptr) {
    ptr->C = 5;
    ptr->K = 4;
    ptr->E = 3;
    ptr->T = 200;
    ptr->P = 100;
    ptr->S = 20;
    ptr->L = 5;
    ptr->J = 2;
    ptr->A = 400;
    ptr->S1 = 2;
    ptr->S2 = 10;

    return 0;
}
