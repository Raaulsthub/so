#include "so.h"
#include "tela.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdio.h>

// quantos programas no so
#define PROGRAMAS 5
// quantum
#define QUANTUM 400
// round robin
#define ROUND 0

// STRUCTS

// escalonador 1
typedef struct escalonador_t
{
  int ready[PROGRAMAS]; // fifo vai ser implementada com um vetor
  int average_time[PROGRAMAS];
  int last_time[PROGRAMAS];
  int quantum[PROGRAMAS];
  int curr_progr_time;
} esc_t;

// instancia de programa
typedef struct programa_t
{
  int size;
  int *code;
} prog_t;

// sistema operacional
struct so_t
{
  contr_t *contr;
  bool paniquei;
  cpu_estado_t *cpue;
  tab_t *tabela;
  prog_t **programs;
  int proc_count;
  int curr_prog;
  esc_t *esc;
  FILE *file;

  int curr_quadro;

  // metricas gerais t2
  long ciclos_totais;
  long ciclos_zumbi;
  long interrupcoes_totais;
  // metricas por processo t2
  long retorno[PROGRAMAS];
  long bloqueado[PROGRAMAS];
  long executando[PROGRAMAS];
  long espera[PROGRAMAS];
  // resp media = tempo block / n blocks
  long bloqueios[PROGRAMAS];
  long preemps[PROGRAMAS];
};

// infos de processo guardadas na tabela
struct process_info
{
  cpu_estado_t *cpue;
  int id;
  int estado;
  int disp;
  acesso_t acesso;
  tab_pag_t *tabela_paginas;
};

// tabela de processos
struct tabela_proc
{
  int n_procs;
  proc_t **processes;
};

// funcoes
void so_atualiza_tab(so_t *self, int motivo);
// static void init_mem(so_t *self);
static void panico(so_t *self);
void init_proc(so_t *self);
int esc_seleciona_processo(so_t *self);
void so_atualiza_tab(so_t *self, int motivo);
void esc_retira_pronto(so_t *self);
void esc_adiciona_pronto(so_t *self, int program_idx);

// SISTEMA OPERACIONAL (CORE)
so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL)
    return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->proc_count = 0;
  self->curr_prog = 0;
  self->curr_quadro = 0;
  self->file = fopen("saida.txt", "w");

  // metricas
  for (int i = 0; i < PROGRAMAS; i++)
  {
    self->bloqueado[i] = 0;
    self->executando[i] = 0;
    self->espera[i] = 0;
    self->retorno[i] = 0;
  }

  // ROUND ROBIN E PROCESSO MAIS CURTO
  self->esc = (esc_t *)malloc(sizeof(esc_t));
  for (int i = 0; i < PROGRAMAS; i++)
  {
    self->esc->ready[i] = -1;
    self->esc->average_time[i] = 0;
    self->esc->last_time[i] = 0;
  }
  self->esc->curr_progr_time = 0;

  // TABELA DE PROCESSOS
  self->tabela = (tab_t *)malloc(sizeof(tab_t));
  self->tabela->processes = (proc_t **)malloc(sizeof(proc_t *) * PROGRAMAS);
  self->programs = (prog_t **)malloc(sizeof(prog_t *) * PROGRAMAS);

  // PROGRAMAS
  int progr0[] = {
    #include "init.maq"
  };
  self->programs[0] = (prog_t *)malloc(sizeof(prog_t));
  self->programs[0]->size = sizeof(progr0) / sizeof(int);
  self->programs[0]->code = (int *)malloc(sizeof(int) * self->programs[0]->size);
  for (int i = 0; i < self->programs[0]->size; i++)
  {
    self->programs[0]->code[i] = progr0[i];
  }
  int progr1[] = {
    #include "p1.maq"
  };
  self->programs[1] = (prog_t *)malloc(sizeof(prog_t));
  self->programs[1]->size = sizeof(progr1) / sizeof(int);
  self->programs[1]->code = (int *)malloc(sizeof(int) * self->programs[1]->size);
  for (int i = 0; i < self->programs[1]->size; i++)
  {
    self->programs[1]->code[i] = progr1[i];
  }
  int progr2[] = {
    #include "p2.maq"
  };
  self->programs[2] = (prog_t *)malloc(sizeof(prog_t));
  self->programs[2]->size = sizeof(progr2) / sizeof(int);
  self->programs[2]->code = (int *)malloc(sizeof(int) * self->programs[2]->size);
  for (int i = 0; i < self->programs[2]->size; i++)
  {
    self->programs[2]->code[i] = progr2[i];
  }
  int progr3[] = {
    #include "a1.maq"
  };
  self->programs[3] = (prog_t *)malloc(sizeof(prog_t));
  self->programs[3]->size = sizeof(progr3) / sizeof(int);
  self->programs[3]->code = (int *)malloc(sizeof(int) * self->programs[3]->size);
  for (int i = 0; i < self->programs[3]->size; i++)
  {
    self->programs[3]->code[i] = progr3[i];
  }

  init_proc(self);
  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

// processo 0 - init do so
void init_proc(so_t *self)
{
  int id = self->proc_count;
  self->proc_count++;

  // inicializando cpue
  cpu_estado_t *cpu = cpue_cria();
  int estado = EM_EXECUCAO;

  // salvando na tabela infos do processo init
  self->tabela->processes[0] = (proc_t *)malloc(sizeof(proc_t));
  self->tabela->processes[0]->id = id;
  self->tabela->processes[0]->estado = estado;
  self->tabela->processes[0]->cpue = cpu;

  // tabela de pag
  int quadros = (self->programs[0]->size / TAM_QUADRO);
  if (self->programs[0]->size % TAM_QUADRO != 0)
    quadros++;
  t_printf(" QUADROS %d", quadros);
  self->tabela->processes[id]->tabela_paginas = tab_pag_inicializa(self->curr_quadro, quadros, TAM_QUADRO);
  self->curr_quadro += quadros;

  // memoria
  mmu_usa_tab_pag(contr_mmu(self->contr), self->tabela->processes[0]->tabela_paginas);
  for (int i = 0; i < self->programs[0]->size; i++)
  {
    mmu_escreve(contr_mmu(self->contr), i, self->programs[0]->code[i]);
  }

  for (int i = 0; i < MEM_TAM; i++)
  {
    int a;
    mem_le(contr_mem(self->contr), i, &a);
    fprintf(self->file, "%d ", a);
  }
  fprintf(self->file, "\n");
}

// sisop le
static void so_trata_sisop_le(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  if (err != ERR_OK)
  {
    int idx = self->curr_prog;
    self->tabela->processes[idx]->acesso = leitura;
    self->tabela->processes[idx]->disp = disp;
    cpue_muda_erro(self->cpue, err, 0);
  }
  else
  {
    cpue_muda_X(self->cpue, val);
  }

  cpue_muda_A(self->cpue, err);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// sisop escreve
static void so_trata_sisop_escr(so_t *self)
{
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  if (err != ERR_OK)
  {
    int idx = self->curr_prog;
    self->tabela->processes[idx]->disp = disp;
    self->tabela->processes[idx]->acesso = escrita;

    cpue_muda_erro(self->cpue, err, 0);
  }

  cpue_muda_A(self->cpue, err);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_bota_proc(so_t *self)
{
  self->esc->curr_progr_time = 0;
  int idx = esc_seleciona_processo(self);

  if (idx != -1)
  {
    self->tabela->processes[idx]->estado = EM_EXECUCAO;
    self->curr_prog = idx;
    cpue_copia(self->tabela->processes[idx]->cpue, self->cpue);
    mmu_usa_tab_pag(contr_mmu(self->contr), self->tabela->processes[idx]->tabela_paginas);
    cpue_muda_modo(self->cpue, usuario);

    // ROUND ROBIN
    esc_retira_pronto(self);

    t_printf("PROGRAMA %d SENDO EXECUTADO! PC = %d", idx, cpue_PC(self->cpue));
  }
  else
  {
    // botar cpu em zumbi
    self->curr_prog = -1;
    cpue_muda_modo(self->cpue, zumbi);
  }
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  int id;
  int i = self->curr_prog;
  self->tabela->processes[i]->estado = FINALIZADO;

  id = self->tabela->processes[i]->id;
  t_printf("PROCESSO %d FINALIZADO", id);
  so_bota_proc(self);
  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  // incrementa o PC
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
  // int prog_number = cpue_A(self->cpue);
  //  inicializando cpue
  cpu_estado_t *cpu = cpue_cria();
  int id = self->proc_count;
  self->proc_count++;
  int estado = PRONTO;

  // salvando na tabela
  self->tabela->processes[id] = (proc_t *)malloc(sizeof(proc_t));
  self->tabela->processes[id]->id = id;
  self->tabela->processes[id]->estado = estado;
  self->tabela->processes[id]->cpue = cpu;

  // tabela de pag
  int quadros = (self->programs[id]->size / TAM_QUADRO);
  if (self->programs[0]->size % TAM_QUADRO != 0)
    quadros++;
  self->tabela->processes[id]->tabela_paginas = tab_pag_inicializa(self->curr_quadro, quadros, TAM_QUADRO);
  self->curr_quadro += quadros;

  // memoria
  mmu_usa_tab_pag(contr_mmu(self->contr), self->tabela->processes[id]->tabela_paginas);
  for (int i = 0; i < self->programs[id]->size; i++)
  {
    mmu_escreve(contr_mmu(self->contr), i, self->programs[id]->code[i]);
  }
  // volta pro init
  mmu_usa_tab_pag(contr_mmu(self->contr), self->tabela->processes[0]->tabela_paginas);

  cpue_muda_erro(self->cpue, ERR_OK, 0);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);

  // ROUND ROBIN
  esc_adiciona_pronto(self, id);
  self->esc->quantum[id] = QUANTUM;

  // debug memoria
  for (int i = 0; i < MEM_TAM; i++)
  {
    int a;
    mem_le(contr_mem(self->contr), i, &a);
    fprintf(self->file, "%d ", a);
  }
  fprintf(self->file, "\n");

  t_printf("PROCESSO %d CRIADO", id);
}

static void so_trata_sisop(so_t *self)
{
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  so_chamada_t chamada = cpue_complemento(self->cpue);
  switch (chamada)
  {
  case SO_LE:
    so_trata_sisop_le(self);
    break;
  case SO_ESCR:
    so_trata_sisop_escr(self);
    break;
  case SO_FIM:
    so_trata_sisop_fim(self);
    break;
  case SO_CRIA:
    so_trata_sisop_cria(self);
    break;
  default:
    t_printf("so: chamada de sistema nao reconhecida %d\n", chamada);
    panico(self);
    break;
  }
}

static void so_trata_tic(so_t *self)
{
  // metricas t2
  self->ciclos_totais++;
  if (self->curr_prog == -1)
  {
    self->ciclos_zumbi++;
  }

  // round robin nao atua no proc 0
  if (self->curr_prog != 0)
  {
    // ROUND ROBIN
    if (self->curr_prog != -1)
    {
      self->esc->quantum[self->curr_prog]--;
    }
    // preempcao
    if (self->esc->quantum[self->curr_prog] == 0)
    {
      cpue_muda_modo(self->cpue, supervisor);
      cpue_muda_erro(self->cpue, ERR_QUANTUM, 0);
      exec_altera_estado(contr_exec(self->contr), self->cpue);
    }
  }

  for (int i = 0; i < PROGRAMAS; i++)
  {
    if (self->tabela->processes[i] != NULL && self->tabela->processes[i]->estado == BLOCKEADO)
    {
      int disp = self->tabela->processes[i]->disp;
      acesso_t forma = self->tabela->processes[i]->acesso;
      if (es_pronto(contr_es(self->contr), disp, forma))
      {
        self->tabela->processes[i]->estado = PRONTO;
        // ROUND ROBIN
        self->esc->quantum[i] = QUANTUM;
        esc_adiciona_pronto(self, i);

        if (cpue_modo(self->cpue) == zumbi)
        {
          cpue_muda_modo(self->cpue, supervisor);
          cpue_muda_erro(self->cpue, ERR_OK, 0);
          exec_altera_estado(contr_exec(self->contr), self->cpue);
        }
      }
    }
  }
  if (self->curr_prog == -1)
  {
    so_bota_proc(self);
    cpue_muda_erro(self->cpue, ERR_OK, 0);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
  }

  int tem_proc = 0;
  for (int i = 0; i < PROGRAMAS; i++)
  {
    if (self->tabela->processes[i] != NULL && self->tabela->processes[i]->estado != FINALIZADO)
    {
      tem_proc++;
    }
  }

  if (tem_proc == 0)
  {
    t_printf("Fim da execucao dos programas!");
    t_printf("Tempo total: %d\t Uso da Cpu: %d\t Interrupcoes: %d\t", self->ciclos_totais, self->ciclos_totais - self->ciclos_zumbi, self->interrupcoes_totais);
    t_printf("P\tR\tB\tE\tesp\tmr\tb\tpre");
    for (int i = 0; i < PROGRAMAS; i++)
    {
      if (self->retorno[i] != 0)
      {
        float media_resp = 0;
        if (self->bloqueios[i] != 0)
        {
          media_resp = self->bloqueado[i] / self->bloqueios[i];
        }
        t_printf("%d\t%d\t%d\t%d\t%d\t%0.2f\t%d\t%d", i, self->retorno[i], self->bloqueado[i], self->executando[i], self->espera[i], media_resp,
                 self->bloqueios[i], self->preemps[i]);
      }
    }
    self->paniquei = true;
  }

  // tempo do processo sendo executado
  self->esc->curr_progr_time++;

  // metricas t2
  for (int i = 0; i < PROGRAMAS; i++)
  {
    if (self->tabela->processes[i] != NULL)
    {
      if (self->tabela->processes[i]->estado == BLOCKEADO)
      {
        self->bloqueado[i]++;
      }
      if (self->tabela->processes[i]->estado == PRONTO)
      {
        self->espera[i]++;
      }
      if (self->tabela->processes[i]->estado == EM_EXECUCAO)
      {
        self->executando[i]++;
      }
      if (self->tabela->processes[i]->estado != FINALIZADO)
      {
        self->retorno[i]++;
      }
    }
  }
}

void so_trata_disp(so_t *self)
{
  // metricas t2
  self->bloqueios[self->curr_prog]++;
  // parada por leitura/escrita
  if (ROUND == 1)
  {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
  }
  else
  {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
    // primeira vez que o processo esta executando
    self->esc->average_time[self->curr_prog] = (self->esc->curr_progr_time + self->esc->last_time[self->curr_prog]) / 2;
    self->esc->last_time[self->curr_prog] = self->esc->curr_progr_time;
  }
  so_atualiza_tab(self, BLOCKEADO);
  so_bota_proc(self);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_trata_quantum(so_t *self)
{
  // metricas t2
  self->preemps[self->curr_prog]++;
  // quantum acabou
  if (ROUND == 1)
  {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
  }
  else
  {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
    // primeira vez que o processo esta executando
    self->esc->average_time[self->curr_prog] = (self->esc->curr_progr_time + self->esc->last_time[self->curr_prog]) / 2;
    self->esc->last_time[self->curr_prog] = self->esc->curr_progr_time;
  }
  esc_adiciona_pronto(self, self->curr_prog); // vai pro fim da fila
  so_atualiza_tab(self, PRONTO);
  so_bota_proc(self);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_int(so_t *self, err_t err)
{
  switch (err)
  {
  case ERR_SISOP:
    self->interrupcoes_totais++;
    so_trata_sisop(self);
    break;
  case ERR_TIC:
    self->interrupcoes_totais++;
    so_trata_tic(self);
    break;
  case ERR_OCUP:
    self->interrupcoes_totais++;
    so_trata_disp(self);
    break;
  case ERR_QUANTUM:
    self->interrupcoes_totais++;
    self->interrupcoes_totais++;
    so_trata_quantum(self);
    break;
  default:
    t_printf("SO: interrupcao nao tratada [%s]", err_nome(err));
    self->paniquei = true;
  }
}

bool so_ok(so_t *self)
{
  return !self->paniquei;
}

// static void init_mem(so_t *self)
// {
//   mem_t *mem = contr_mem(self->contr);
//   for (int i = 0; i < self->programs[0]->size; i++) {
//     if (mem_escreve(mem, i, self->programs[0]->code[i]) != ERR_OK) {
//       t_printf("so.init_mem: erro de memoria, endereco %d\n", i);
//       panico(self);
//     }
//   }
// }

static void panico(so_t *self)
{
  t_printf("Problema irrecuperavel no SO");
  self->paniquei = true;
}

// FUNCOES DE ESCALONADOR

int esc_seleciona_processo(so_t *self)
{
  // returns -1 if there is no ready process
  if (ROUND == 1)
  {
    return self->esc->ready[0];
  }
  else
  {
    int idx_shortest = -1;
    int shortest_time = -1;
    for (int i = 0; i < PROGRAMAS; i++)
    {
      if (self->esc->ready[i] != -1)
      {
        if (idx_shortest == -1)
        {
          idx_shortest = self->esc->ready[i];
          shortest_time = self->esc->average_time[self->esc->ready[i]];
        }
        else if (self->esc->average_time[self->esc->ready[i]] < shortest_time)
        {
          idx_shortest = self->esc->ready[i];
          shortest_time = self->esc->average_time[self->esc->ready[i]];
        }
      }
    }
    return idx_shortest;
  }
}

void esc_adiciona_pronto(so_t *self, int program_idx)
{
  for (int i = 0; i < PROGRAMAS; i++)
  {
    if (self->esc->ready[i] == -1)
    {
      self->esc->ready[i] = program_idx;
      return;
    }
  }
}

void esc_retira_pronto(so_t *self)
{
  for (int i = 0; i < (PROGRAMAS - 1); i++)
  {
    self->esc->ready[i] = self->esc->ready[i + 1];
  }
  self->esc->ready[PROGRAMAS - 1] = -1;
}

// TABELA

void so_atualiza_tab(so_t *self, int motivo)
{
  int idx_current = self->curr_prog;
  exec_copia_estado(contr_exec(self->contr), self->tabela->processes[idx_current]->cpue);
  self->tabela->processes[idx_current]->estado = motivo;
}
