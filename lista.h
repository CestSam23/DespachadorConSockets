#ifndef LISTA_H
#define LISTA_H
#define MAX 1000
#include "mylib.h"

/*
Definición de cabecera de la estructura de datos Lista
Se usará una lista de tamaño MAX para esto.
En este archivo esta la definición de las funciones
*/

typedef struct{
	int actual;
	int size;
	struct Process procesos[MAX];
}lista_t;

int addProcess(lista_t *lista, struct Process process);
int addProcesses(lista_t *lista, struct Process process[], int n);
process_t deleteProcess(lista_t *lista);
process_t getProcess(lista_t *lista, int n);
int aumentarEspera(lista_t *lista, int s);
int restarEjecucion(lista_t *lista, int s);
int aumentarTerminacion(lista_t *lista, int s);
void ordenarPorPrioridad(lista_t *lista);
int size(lista_t *lista);
int isEmpty(lista_t *lista);
int next(lista_t *lista);
int prev(lista_t *lista);
process_t actual(lista_t *lista);
int actualN(lista_t *lista);
int isLast(lista_t *lista);
void rewindList(lista_t *lista);

void toString(lista_t *lista);
#endif
