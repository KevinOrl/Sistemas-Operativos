/*
productor.c

Programa Productor de Procesos:
- Lee el esquema de memoria desde la memoria compartida
- Genera procesos (threads) con parámetros aleatorios según el esquema
- Cada proceso busca espacio en memoria, duerme y libera
- Usa semáforos para sincronización
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "common.h"
#include "productor.h"

#define MAX_HILOS 10

// Estructura para pasar parámetros a los hilos
typedef struct {
    int semid;
    MemoriaCompartida *mem;
    ParametrosProceso params;
} ThreadArgs;

void *proceso_hilo(void *arg);

void crear_procesos() {
    // 1. Conectarse a memoria compartida y semáforos
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1) { perror("shmat"); exit(1); }
    int semid = semget(SEM_KEY, NUM_SEMS, 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    srand(time(NULL));
    pthread_t hilos[MAX_HILOS];
    ThreadArgs args[MAX_HILOS];

    // 2. Crear hilos con parámetros aleatorios
    for (int i = 0; i < MAX_HILOS; i++) {
        args[i].semid = semid;
        args[i].mem = mem;
        args[i].params.pid = getpid() + i + 1;
        args[i].params.tiempo = 20 + rand() % 41; // 20-60s
        if (mem->esquema == PAGINACION) {
            args[i].params.num_paginas = 1 + rand() % 10;
            args[i].params.num_segmentos = 0;
        } else {
            args[i].params.num_paginas = 0;
            args[i].params.num_segmentos = 1 + rand() % 5;
            for (int j = 0; j < args[i].params.num_segmentos; j++)
                args[i].params.tam_segmentos[j] = 1 + rand() % 3;
        }
        pthread_create(&hilos[i], NULL, proceso_hilo, &args[i]);
        sleep(1 + rand() % 31); // 1-31s entre procesos
    }
    // 3. Esperar a que terminen
    for (int i = 0; i < MAX_HILOS; i++)
        pthread_join(hilos[i], NULL);

    shmdt(mem);
}

// Funcion para la creacion del hilo que simula el proceso, se encarga de 
// buscar espacio en memoria, dormir y liberar
void *proceso_hilo(void *arg) {
    ThreadArgs *targs = (ThreadArgs *)arg;
    int semid = targs->semid;
    MemoriaCompartida *mem = targs->mem;
    ParametrosProceso *params = &targs->params;
    int exito = 0;

    // 1. Buscar espacio en memoria (región crítica)
    sem_p(semid, SEM_MEMORIA);
    if (mem->esquema == PAGINACION) {
        int libres = 0;
        for (int i = 0; i < mem->total_frames; i++)
            if (mem->frames[i].estado == FRAME_LIBRE)
                libres++;
        if (libres >= params->num_paginas) {
            int asignados = 0;
            for (int i = 0; i < mem->total_frames && asignados < params->num_paginas; i++) {
                if (mem->frames[i].estado == FRAME_LIBRE) {
                    mem->frames[i].estado = FRAME_OCUPADO;
                    mem->frames[i].pid = params->pid;
                    mem->frames[i].segmento = -1;
                    asignados++;
                }
            }
            exito = 1;
        }
    } else { // SEGMENTACION
        int total_necesario = 0;
        for (int i = 0; i < params->num_segmentos; i++)
            total_necesario += params->tam_segmentos[i];
        int libres = 0;
        for (int i = 0; i < mem->total_frames; i++)
            if (mem->frames[i].estado == FRAME_LIBRE)
                libres++;
        if (libres >= total_necesario) {
            int asignados = 0;
            for (int i = 0; i < mem->total_frames && asignados < total_necesario; i++) {
                if (mem->frames[i].estado == FRAME_LIBRE) {
                    mem->frames[i].estado = FRAME_OCUPADO;
                    mem->frames[i].pid = params->pid;
                    mem->frames[i].segmento = asignados; // simple
                    asignados++;
                }
            }
            exito = 1;
        }
    }
    sem_v(semid, SEM_MEMORIA);

    // 2. Loguear resultado
    if (exito) {
        log_mensaje(semid, params->pid, "Asignación exitosa");
        sleep(params->tiempo); // Simula ejecución
        // Liberar memoria (región crítica)
        sem_p(semid, SEM_MEMORIA);
        for (int i = 0; i < mem->total_frames; i++) {
            if (mem->frames[i].pid == params->pid) {
                mem->frames[i].estado = FRAME_LIBRE;
                mem->frames[i].pid = -1;
                mem->frames[i].segmento = -1;
            }
        }
        sem_v(semid, SEM_MEMORIA);
        log_mensaje(semid, params->pid, "Liberación de memoria");
    } else {
        log_mensaje(semid, params->pid, "No hay espacio suficiente");
    }
    pthread_exit(NULL);
}

// main simple para pruebas
int main() {
    crear_procesos();
    return 0;
}
