#include "so.h"
#include "tela.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

// quantos programas no so
#define PROGRAMAS 5
// quantum
#define QUANTUM 30
// round robin
#define ROUND 0


// STRUCTS

// escalonador 1
typedef struct escalonador_t{
  int ready[PROGRAMAS]; // fifo vai ser implementada com um vetor
  int average_time[PROGRAMAS];
  int last_time[PROGRAMAS];
  int quantum[PROGRAMAS]; 
  int curr_progr_time;
}esc_t;

// instancia de programa
typedef struct programa_t{
  int size;
  int* code;
}prog_t;

// sistema operacional
struct so_t {
  contr_t *contr;       
  bool paniquei;        
  cpu_estado_t *cpue;   
  tab_t* tabela;
  prog_t** programs;
  int proc_count;
  int curr_prog;
  esc_t* esc;

  // metricas gerais t2
  clock_t begin; // comeco so
  double cpu_parada; // tempo que a cpu ficou em modo zumbi
  long interrupcoes; // numero de interrupcoes tratadas
  clock_t begin_zumbi; // marca o tempo assim que a cpu entra em modo zumbi

  // metricas por processo - AINDA NAO FEITO
  clock_t proc_begin[PROGRAMAS];
  clock_t proc_end[PROGRAMAS];
  clock_t proc_block_begin[PROGRAMAS];
  clock_t proc_exec_begin[PROGRAMAS];
  double proc_blockeado[PROGRAMAS];
  double proc_pronto[PROGRAMAS];
  double proc_exec[PROGRAMAS];
  long bloqueios[PROGRAMAS];
  long preempcoes[PROGRAMAS];
  double resp_media[PROGRAMAS];
  

};

// infos de processo guardadas na tabela
struct process_info {
  mem_t* mem;
  cpu_estado_t* cpue;
  int id;
  int estado;
  int disp;
  acesso_t acesso; 
};

// tabela de processos
struct tabela_proc {
  int n_procs;
  proc_t** processes;
};

// funcoes
void so_atualiza_tab(so_t* self, int motivo);
static void init_mem(so_t *self);
static void panico(so_t *self);
void init_proc(so_t* self);
int esc_seleciona_processo(so_t* self);
void so_atualiza_tab(so_t* self, int motivo);
void esc_retira_pronto(so_t* self);
void esc_adiciona_pronto(so_t* self, int program_idx);

// SISTEMA OPERACIONAL (CORE)
so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->proc_count = 0;
  self->curr_prog = 0;

  for (int i = 0; i < PROGRAMAS; i++) {
    self->proc_blockeado[i] = 0;
    self->proc_pronto[i] = 0;
    self->bloqueios[i] = 0;
    self->preempcoes[i] = 0;
    self->resp_media[i] = 0;
    self->proc_exec[i] = 0;
  }


  // METRICAS T2
  self->begin = clock();

  
  // ROUND ROBIN E PROCESSO MAIS CURTO
  self->esc = (esc_t*) malloc (sizeof(esc_t));
  for (int i = 0; i < PROGRAMAS; i++) {
    self->esc->ready[i] = -1;
    self->esc->average_time[i] = 0;
    self->esc->last_time[i] = 0;
  }
  self->esc->curr_progr_time = 0;

  // TABELA DE PROCESSOS
  self->tabela = (tab_t*) malloc (sizeof(tab_t));
  self->tabela->processes = (proc_t**) malloc (sizeof(proc_t*) * PROGRAMAS);
  self->programs = (prog_t**) malloc (sizeof(prog_t*) * PROGRAMAS);


  // PROGRAMAS
  int progr0[] = {
    #include "init.maq"
  };
  self->programs[0] = (prog_t*) malloc (sizeof(prog_t));
  self->programs[0]->size = sizeof(progr0)/sizeof(int);
  self->programs[0]->code = (int*) malloc (sizeof(int) * self->programs[0]->size);
  for (int i = 0; i < self->programs[0]->size; i++) {
    self->programs[0]->code[i] = progr0[i];
  }
  int progr1[] = {
    #include "p1.maq"
  };
  self->programs[1] = (prog_t*) malloc (sizeof(prog_t));
  self->programs[1]->size = sizeof(progr1)/sizeof(int);
  self->programs[1]->code = (int*) malloc (sizeof(int) * self->programs[1]->size);
  for (int i = 0; i < self->programs[1]->size; i++) {
    self->programs[1]->code[i] = progr1[i];
  }
  int progr2[] = {
    #include "p2.maq"
  };
  self->programs[2] = (prog_t*) malloc (sizeof(prog_t));
  self->programs[2]->size = sizeof(progr2)/sizeof(int);
  self->programs[2]->code = (int*) malloc (sizeof(int) * self->programs[2]->size);
  for (int i = 0; i < self->programs[2]->size; i++) {
    self->programs[2]->code[i] = progr2[i];
  }


  init_proc(self);
  init_mem(self);
  return self;
}

void so_destroi(so_t *self)
{
  cpue_destroi(self->cpue);
  free(self);
}

// processo 0 - init do so
void init_proc(so_t* self) {
  // iniciando memoria
  mem_t* mem = mem_cria(MEM_TAM);
  for (int i = 0; i < self->programs[0]->size; i++) {
    mem_escreve(mem, i, self->programs[0]->code[i]);
  }
  // inicializando cpue
  cpu_estado_t* cpu = cpue_cria();
  int id = self->proc_count;
  self->proc_count++;
  int estado = EM_EXECUCAO;

  // salvando na tabela infos do processo init
  self->tabela->processes[0] = (proc_t*)malloc(sizeof(proc_t));
  self->tabela->processes[0]->id = id;
  self->tabela->processes[0]->estado = estado;
  self->tabela->processes[0]->cpue = cpu;
  self->tabela->processes[0]->mem = mem;
}


// sisop le
static void so_trata_sisop_le(so_t *self)
{
  self->interrupcoes++;
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  if (err != ERR_OK) {
    int idx = self->curr_prog;
    self->tabela->processes[idx]->acesso = leitura;
    self->tabela->processes[idx]->disp = disp;
    cpue_muda_erro(self->cpue, err, 0);
  } else{
    cpue_muda_X(self->cpue, val);
  }

  cpue_muda_A(self->cpue, err);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// sisop escreve
static void so_trata_sisop_escr(so_t *self)
{
  self->interrupcoes++;
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);

  cpue_muda_erro(self->cpue, ERR_OK, 0);

  if (err != ERR_OK) {
    int idx = self->curr_prog;
    self->tabela->processes[idx]->disp = disp;
    self->tabela->processes[idx]->acesso = escrita;

    cpue_muda_erro(self->cpue, err, 0);
  }

  cpue_muda_A(self->cpue, err);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue) + 2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_bota_proc(so_t* self) {
  self->esc->curr_progr_time = 0;
  int idx = esc_seleciona_processo(self);

  if (self->curr_prog == -1 && idx != -1) {
    self->cpu_parada += (double) (clock() - self->begin_zumbi) / CLOCKS_PER_SEC;
  }

  if (idx != -1) {
    self->proc_exec_begin[idx] = clock();
    self->tabela->processes[idx]->estado = EM_EXECUCAO;
    self->curr_prog = idx;
    cpue_copia(self->tabela->processes[idx]->cpue, self->cpue);
    
    mem_t *mem = contr_mem(self->contr);
    for (int i = 0; i < mem_tam(self->tabela->processes[idx]->mem); i++) {
      int val;
      mem_le(self->tabela->processes[idx]->mem, i, &val);
      mem_escreve(mem, i, val);
    }
    cpue_muda_modo(self->cpue, usuario);

    // ROUND ROBIN
    esc_retira_pronto(self);

    t_printf("PROGRAMA %d SENDO EXECUTADO! PC = %d", idx, cpue_PC(self->cpue));

  }
  else {
    if (self->curr_prog != -1) {
      self->begin_zumbi = clock();
    }
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
  self->interrupcoes++;
  int id;
  int i = self->curr_prog;
  self->tabela->processes[i]->estado = FINALIZADO;

  // metricas
  self->proc_end[i] = clock();

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
  self->interrupcoes++;
  int prog_number = cpue_A(self->cpue);   
  // inicializando memoria com o programa
  mem_t* mem = mem_cria(MEM_TAM);
  for (int i = 0; i < self->programs[prog_number]->size; i++) {
    mem_escreve(mem, i, self->programs[prog_number]->code[i]);
  }
  // inicializando cpue
  cpu_estado_t* cpu = cpue_cria();
  int id = self->proc_count;
  self->proc_count++;
  int estado = PRONTO;

  // salvando na tabela
  self->tabela->processes[id] = (proc_t*)malloc(sizeof(proc_t));
  self->tabela->processes[id]->id = id;
  self->tabela->processes[id]->estado = estado;
  self->tabela->processes[id]->cpue = cpu;
  self->tabela->processes[id]->mem = mem;

  cpue_muda_erro(self->cpue, ERR_OK, 0);
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  exec_altera_estado(contr_exec(self->contr), self->cpue);

  // ROUND ROBIN
  esc_adiciona_pronto(self, id);
  self->esc->quantum[id] = QUANTUM;

  // metricas
  self->proc_begin[id] = clock();

  t_printf("PROCESSO %d CRIADO", id);
}

static void so_trata_sisop(so_t *self)
{
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  so_chamada_t chamada = cpue_complemento(self->cpue);
  switch (chamada) {
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
  // round robin nao atua no proc 0
  if (self->curr_prog != 0) {
    // ROUND ROBIN
    if (self->curr_prog != -1) {
      self->esc->quantum[self->curr_prog]--;
    }
    // preempcao
    if (self->esc->quantum[self->curr_prog] == 0) {
      self->preempcoes[self->curr_prog]++;
      cpue_muda_modo(self->cpue, supervisor);
      cpue_muda_erro(self->cpue, ERR_QUANTUM, 0);
      exec_altera_estado(contr_exec(self->contr), self->cpue);
    }
  } 

  for (int i = 0; i < PROGRAMAS; i++) {
    if (self->tabela->processes[i] != NULL && self->tabela->processes[i]->estado == BLOCKEADO) {
      int disp = self->tabela->processes[i]->disp;
      acesso_t forma = self->tabela->processes[i]->acesso;
      if(es_pronto(contr_es(self->contr), disp, forma)) {
        self->tabela->processes[i]->estado = PRONTO;
        self->proc_blockeado[i] += (double) (clock() - self->proc_block_begin[i]) / CLOCKS_PER_SEC;
        // ROUND ROBIN
        self->esc->quantum[i] = QUANTUM;
        esc_adiciona_pronto(self, i);

        if (cpue_modo(self->cpue) == zumbi) {
          cpue_muda_modo(self->cpue, supervisor);
          cpue_muda_erro(self->cpue, ERR_OK, 0);
          exec_altera_estado(contr_exec(self->contr), self->cpue);
        }
      }
    }
  }
  if (self->curr_prog == -1) {
    so_bota_proc(self);
    cpue_muda_erro(self->cpue, ERR_OK, 0);
    exec_altera_estado(contr_exec(self->contr), self->cpue);
  }

  int tem_proc = 0;
  for (int i = 0; i < PROGRAMAS; i++) {
    if(self->tabela->processes[i] != NULL && self->tabela->processes[i]->estado != FINALIZADO) {
      tem_proc++;
    }
  }

  if (tem_proc == 0) {
    t_printf("Fim da execucao dos programas!");
    t_printf("Tempo total do so: %lfs\t\tTempo de uso da cpu: %lfs",
      (double) (clock() - self->begin) / CLOCKS_PER_SEC, (double) ((clock() - self->begin) / CLOCKS_PER_SEC) - self->cpu_parada);
    t_printf("P\tt\t\tblocks  preemp    \tB\t\tE\t\tP");
    for (int i = 0; i <  PROGRAMAS; i++) {
      if(self->tabela->processes[i] != NULL && self->tabela->processes[i]->estado == FINALIZADO) {
        double temp_exec;
        double temp_p;
        if(i == 0) {
          temp_exec = (double)(self->proc_end[i] - self->proc_begin[i]) / CLOCKS_PER_SEC;
          temp_p = 0;
        } else {
          temp_exec = self->proc_exec[i];
          temp_p = ((double)(self->proc_end[i] - self->proc_begin[i]) / CLOCKS_PER_SEC) - self->proc_exec[i] - self->proc_blockeado[i]; // PRINTAR
        }
        t_printf("%d\t%lfs\t%ld\t%ld\t\t%lfs\t%lfs\t%lfs\t%lfs", i,
          (double)(self->proc_end[i] - self->proc_begin[i]) / CLOCKS_PER_SEC, self->bloqueios[i], self->preempcoes[i], self->proc_blockeado[i], temp_exec, temp_p);
      }
    }

    self->paniquei = true;
  }

  // tempo do processo sendo executado
  self->esc->curr_progr_time++;
}

void so_trata_disp(so_t* self) {
  // parada por leitura/escrita
  if (ROUND == 1) {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
  }
  else {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
    // primeira vez que o processo esta executando
    self->esc->average_time[self->curr_prog] = (self->esc->curr_progr_time + self->esc->last_time[self->curr_prog]) / 2;
    self->esc->last_time[self->curr_prog] = self->esc->curr_progr_time;
  }
  so_atualiza_tab(self, BLOCKEADO);
  self->proc_block_begin[self->curr_prog] = clock();
  self->bloqueios[self->curr_prog]++;
  so_bota_proc(self);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_trata_quantum(so_t* self) {
  self->interrupcoes++;
  // quantum acabou
  if (ROUND == 1) {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
  }
  else {
    self->esc->quantum[self->curr_prog] = QUANTUM; // refill
    // primeira vez que o processo esta executando
    self->esc->average_time[self->curr_prog] = (self->esc->curr_progr_time + self->esc->last_time[self->curr_prog]) / 2;
    self->esc->last_time[self->curr_prog] = self->esc->curr_progr_time;
  }
  esc_adiciona_pronto(self, self->curr_prog); // vai pro fim da fila
  so_atualiza_tab(self, PRONTO);
  self->proc_exec[self->curr_prog] += (double) (clock() - self->proc_exec_begin[self->curr_prog]) / CLOCKS_PER_SEC;
  so_bota_proc(self);
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

void so_int(so_t *self, err_t err)
{
  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    case ERR_OCUP:
      so_trata_disp(self);
      break;
    case ERR_QUANTUM:
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


static void init_mem(so_t *self)
{
  mem_t *mem = contr_mem(self->contr);
  for (int i = 0; i < self->programs[0]->size; i++) {
    if (mem_escreve(mem, i, self->programs[0]->code[i]) != ERR_OK) {
      t_printf("so.init_mem: erro de memoria, endereco %d\n", i);
      panico(self);
    }
  }
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperavel no SO");
  self->paniquei = true;
}


// FUNCOES DE ESCALONADOR

int esc_seleciona_processo(so_t* self) {  
  // returns -1 if there is no ready process
  if (ROUND == 1) {
    return self->esc->ready[0];
  }
  else {
    int idx_shortest = -1;
    int shortest_time = -1;
    for (int i = 0; i < PROGRAMAS; i++){
      if (self->esc->ready[i] != -1) {
        if(idx_shortest == -1) {
          idx_shortest = self->esc->ready[i];
          shortest_time = self->esc->average_time[self->esc->ready[i]];
        }
        else if (self->esc->average_time[self->esc->ready[i]] < shortest_time) {
          idx_shortest = self->esc->ready[i];
          shortest_time = self->esc->average_time[self->esc->ready[i]];
        }
      }
    }
    return idx_shortest;
  }
}

void esc_adiciona_pronto(so_t* self, int program_idx) {
  for (int i = 0; i < PROGRAMAS; i++) {
    if (self->esc->ready[i] == -1) {
      self->esc->ready[i] = program_idx;
      return;
    }
  }
}

void esc_retira_pronto(so_t* self) {
  for(int i = 0; i < (PROGRAMAS - 1); i++) {
    self->esc->ready[i] = self->esc->ready[i+1];
  }
  self->esc->ready[PROGRAMAS-1] = -1;
}

// TABELA

void so_atualiza_tab(so_t* self, int motivo) {
  int idx_current = self->curr_prog;
  cpue_copia(self->cpue, self->tabela->processes[idx_current]->cpue);
  mem_t* mem_atual = contr_mem(self->contr);
  for (int i = 0; i < MEM_TAM; i++) {
    int valor;
    mem_le(mem_atual, i, &valor);
    mem_escreve(self->tabela->processes[idx_current]->mem, i, valor);
  }
  self->tabela->processes[idx_current]->estado = motivo;
}