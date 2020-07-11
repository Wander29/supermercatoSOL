#include <parser_config.h>

int get_params_from_file(param_t *ptr) {
    ptr->C = 3;
    ptr->K = 3;
    ptr->E = 3;
    ptr->T = 200;
    ptr->P = 4;
    ptr->S = 20;
    ptr->L = 10;
    ptr->J = 2;
    ptr->S1 = 2;
    ptr->S2 = 10;

    return 0;
}
