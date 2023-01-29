#include "random.h"
#include <stdio.h>
#include <stdlib.h>



struct rand {
    int number;
};

struct rand* random_cria(void) {
    struct rand* self;
    self = (struct rand*) malloc(sizeof(struct rand));
    if (self != NULL) self->number = -1;
    return self;
}

void random_destroi(struct rand* self) {
    free(self);
}

int random_number(struct rand* self) {
    return self->number;
}

void random_gera_aux(struct rand* self) {
    self->number = rand()%100;
}

err_t random_gera(void* disp, int id, int *pvalor) {
    struct rand* self = disp;
    err_t err = ERR_OK;
    random_gera_aux(self);
            *pvalor = self->number;
    return err;
}



