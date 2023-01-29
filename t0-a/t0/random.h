#ifndef RAND_H
#define RAND_H 

// Dispositivo que gera numeros aleatorios

#include "err.h"

struct rand;

// Cria o dispositivo
// Retorna nulo em caso de erro
struct rand* random_cria(void);

// Destroi o dispositivo
// Não poderao mais ser feitas leituras do dispositivo após essa chamada
void random_destroi(struct rand* self);

// Retorna ultimo numero gerado (guardado na struct do dispositivo)
int random_number(struct rand* self);

// Gera numero randomigo e retorna no ponteiro pvalor
// Possiveis erros sao retornados na funcao
err_t random_gera(void *disp, int id, int *pvalor);

#endif