#include "so.h"
#include "tela.h"
#include <stdlib.h>
#include <stdio.h>

#define PROGRAMAS 5

typedef struct programa_t{
  int size;
  int* code;
}prog_t;

struct so_t {
  contr_t *contr;       // o controlador do hardware
  bool paniquei;        // apareceu alguma situação intratável
  cpu_estado_t *cpue;   // cópia do estado da CPU
  tab_t* tabela;
  prog_t** programs;
  int proc_count;
};

struct process_info {
  mem_t* mem;
  cpu_estado_t* cpue;
  int id;
  int estado;
};

struct tabela_proc {
  int n_procs;
  proc_t** processes;
};

// funções auxiliares
static void init_mem(so_t *self);
static void panico(so_t *self);

// processo 0 - init
void init_proc(so_t* self) {
  // inicializando memoria com o programa
  mem_t* mem = mem_cria(MEM_TAM);
  for (int i = 0; i < self->programs[0]->size; i++) {
    mem_escreve(mem, i, self->programs[0]->code[i]);
  }
  // inicializando cpue
  cpu_estado_t* cpu = cpue_cria();
  int id = self->proc_count;
  self->proc_count++;
  int estado = PRONTO;

  // salvando na tabela
  self->tabela->processes[0] = (proc_t*)malloc(sizeof(proc_t));
  self->tabela->processes[0]->id = id;
  self->tabela->processes[0]->estado = estado;
  self->tabela->processes[0]->cpue = cpu;
  self->tabela->processes[0]->mem = mem;
}

so_t *so_cria(contr_t *contr)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;
  self->contr = contr;
  self->paniquei = false;
  self->cpue = cpue_cria();
  self->proc_count = 0;
  // coloca a CPU em modo usuário
  /*
  exec_copia_estado(contr_exec(self->contr), self->cpue);
  cpue_muda_modo(self->cpue, usuario);
  exec_altera_estado(contr_exec(self->contr), self->cpue);
  */

  self->tabela = (tab_t*) malloc (sizeof(tab_t));
  self->tabela->processes = (proc_t**) malloc (sizeof(proc_t*) * PROGRAMAS);

  self->programs = (prog_t**) malloc (sizeof(prog_t*) * PROGRAMAS);

  int progr0[] = {
    #include "init.maq"
  };
  self->programs[0] = (prog_t*) malloc (sizeof(prog_t));
  self->programs[0]->size = sizeof(progr0)/sizeof(int);
  self->programs[0]->code = (int*) malloc (sizeof(int) * self->programs[0]->size);
  for (int i = 0; i < self->programs[0]->size; i++) {
    self->programs[0]->code[i] = progr0[i];
  }

  // programa para executar na nossa CPU
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

// trata chamadas de sistema

// chamada de sistema para leitura de E/S
// recebe em A a identificação do dispositivo
// retorna em X o valor lido
//            A o código de erro
static void so_trata_sisop_le(so_t *self)
{
  // faz leitura assíncrona.
  // deveria ser síncrono, verificar es_pronto() e bloquear o processo
  int disp = cpue_A(self->cpue);
  int val;
  err_t err = es_le(contr_es(self->contr), disp, &val);
  cpue_muda_A(self->cpue, err);
  if (err == ERR_OK) {
    cpue_muda_X(self->cpue, val);
  }
  // incrementa o PC
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  // altera o estado da CPU (deveria alterar o estado do processo)
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para escrita de E/S
// recebe em A a identificação do dispositivo
//           X o valor a ser escrito
// retorna em A o código de erro
static void so_trata_sisop_escr(so_t *self)
{
  // faz escrita assíncrona.
  // deveria ser síncrono, verificar es_pronto() e bloquear o processo
  int disp = cpue_A(self->cpue);
  int val = cpue_X(self->cpue);
  err_t err = es_escreve(contr_es(self->contr), disp, val);
  cpue_muda_A(self->cpue, err);
  // interrupção da cpu foi atendida
  cpue_muda_erro(self->cpue, ERR_OK, 0);
  // incrementa o PC
  cpue_muda_PC(self->cpue, cpue_PC(self->cpue)+2);
  // altera o estado da CPU (deveria alterar o estado do processo)
  exec_altera_estado(contr_exec(self->contr), self->cpue);
}

// chamada de sistema para término do processo
static void so_trata_sisop_fim(so_t *self)
{
  for(int i = 0; i < PROGRAMAS; i++) {
    if(self->tabela->processes[i]->estado == EM_EXECUCAO) {
      cpue_destroi(self->tabela->processes[i]->cpue);
      mem_destroi(self->tabela->processes[i]->mem);
      self->tabela->processes[i]->estado = FINALIZADO;
      break;
    }
  }
  // AQUI AQUI AQUI AQUI FALTA BOTAR O PROCESSO DEVOLTA

  t_printf("SISOP FIM não implementado");
  panico(self);
}


// chamada de sistema para criação de processo
static void so_trata_sisop_cria(so_t *self)
{
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
}

// trata uma interrupção de chamada de sistema
static void so_trata_sisop(so_t *self)
{
  // o tipo de chamada está no "complemento" do cpue
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
      t_printf("so: chamada de sistema não reconhecida %d\n", chamada);
      panico(self);
  }
}

// trata uma interrupção de tempo do relógio
static void so_trata_tic(so_t *self)
{
  // TODO: tratar a interrupção do relógio
}

// houve uma interrupção do tipo err — trate-a
void so_int(so_t *self, err_t err)
{
  switch (err) {
    case ERR_SISOP:
      so_trata_sisop(self);
      break;
    case ERR_TIC:
      so_trata_tic(self);
      break;
    default:
      t_printf("SO: interrupção não tratada [%s]", err_nome(err));
      self->paniquei = true;
  }
}

// retorna false se o sistema deve ser desligado
bool so_ok(so_t *self)
{
  return !self->paniquei;
}


// carrega um programa na memória
static void init_mem(so_t *self)
{
  // inicializa a memória com o programa 
  mem_t *mem = contr_mem(self->contr);
  for (int i = 0; i < self->programs[0]->size; i++) {
    if (mem_escreve(mem, i, self->programs[0]->code[i]) != ERR_OK) {
      t_printf("so.init_mem: erro de memória, endereco %d\n", i);
      panico(self);
    }
  }
}
  
static void panico(so_t *self) 
{
  t_printf("Problema irrecuperável no SO");
  self->paniquei = true;
}
