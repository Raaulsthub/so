#ifndef SO_H
#define SO_H

// SO
// Simula o sistema operacional
// É chamado na inicialização (quando deve colocar um programa na memória), e
//   depois só quando houver interrupção
//
// Por enquanto não faz quase nada
#define PRONTO 0
#define EM_EXECUCAO 1
#define FINALIZADO 2
#define BLOCKEADO_LEITURA 3

typedef struct so_t so_t;
typedef struct tabela_proc tab_t;
typedef struct process_info proc_t;

// as chamadas de sistema
typedef enum {
  SO_LE = 1,       // lê do dispositivo em A; coloca valor lido em X
  SO_ESCR,         // escreve o valor em X no dispositivo em A
  SO_FIM,          // encerra a execução do processo
  SO_CRIA,         // cria um novo processo, para executar o programa A
} so_chamada_t;

#include "contr.h"
#include "err.h"
#include <stdbool.h>

so_t *so_cria(contr_t *contr);

void so_destroi(so_t *self);

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err);

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self);

#endif // SO_H
