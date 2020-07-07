#include <parser_config.h>

int get_params_from_file(param_t *ptr) {
    ptr->C = 50;
    ptr->K = 6;
    ptr->E = 3;
    ptr->T = 200;
    ptr->P = 100;
    ptr->S = 20;
    ptr->J = 2;

    return 0;
}
