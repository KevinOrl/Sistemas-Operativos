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
    // retorna un entero mayor a 0 si se pudo conectar, sino termina el programa con error
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);
    
    //validacion de error del shmget, si no se pudo conectar a la memoria compartida, se muestra un mensaje de error y se termina el programa
    if (shmid < 0) { perror("shmget"); exit(1); }

    // Mapea la memoria compartida para acceder a ella
    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    
    // validacion de error del shmget
    if (mem == (void *)-1) { 
        perror("shmat"); exit(1); 
    }
    // esto hace que el proceso se quede conectado a la memoria 
    // compartida, lo que es necesario para que los hilos puedan 
    //acceder a ella. Si no se hiciera esto, los hilos no podrían
    // acceder a la memoria compartida y el programa no funcionaría
    // correctamente.
    int semid = semget(SEM_KEY, NUM_SEMS, 0666);
    if (semid < 0) { perror("semget"); exit(1); }

    srand(time(NULL));
    // se crea arreglo de hilos y argumentos para cada hilo
    pthread_t hilos[MAX_HILOS];
    ThreadArgs args[MAX_HILOS];

    // 2. Crear hilos con parámetros aleatorios
    // Se le asigna informacion aleatoria a cada proceso
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
            // recorre cada segmento y le asigna un tamaño aleatorio
            // entre 1 y 3
            for (int j = 0; j < args[i].params.num_segmentos; j++)
                args[i].params.tam_segmentos[j] = 1 + rand() % 3;
        }
        //Se va creando cada proceso con su respectiva informacion, 
        // y se le asigna un tiempo de ejecucion aleatorio entre 20
        // y 60 segundos, y dependiendo del esquema de memoria, 
        //se le asigna un numero aleatorio de paginas o segmentos.
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
    //Se trae informacion de cada proceso desde los argumentos del hilo,
    //incluyendo el semáforo, la memoria compartida y los parámetros del proceso.
    ThreadArgs *targs = (ThreadArgs *)arg;
    int semid = targs->semid;
    MemoriaCompartida *mem = targs->mem;
    ParametrosProceso *params = &targs->params;
    int exito = 0;
    int tabla_idx = -1;

    /* Registrar entrada del proceso en la tabla de procesos */
    sem_p(semid, SEM_TABLA);
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (mem->procesos[i].estado == PROC_VACIO) {
            tabla_idx = i;
            mem->procesos[i].pid = params->pid;
            mem->procesos[i].estado = PROC_BUSCANDO;
            mem->procesos[i].tiempo = params->tiempo;
            mem->procesos[i].num_paginas = params->num_paginas;
            mem->procesos[i].num_segmentos = params->num_segmentos;
            mem->procesos[i].num_frames_asignados = 0;
            for (int f = 0; f < MAX_FRAMES_POR_PROC; f++) mem->procesos[i].frames_asignados[f] = -1;
            for (int s = 0; s < params->num_segmentos; s++)
                mem->procesos[i].tam_segmentos[s] = params->tam_segmentos[s];
            break;
        }
    }
    sem_v(semid, SEM_TABLA);

    // 1. Buscar espacio en memoria (región crítica)
    sem_p(semid, SEM_MEMORIA);
    if (mem->esquema == PAGINACION) {
        int libres = 0;
        //For para saber la cantidad de frames libres 
        for (int i = 0; i < mem->total_frames; i++)
            if (mem->frames[i].estado == FRAME_LIBRE)
                libres++;
        //Si hay suficientes frames libres para asignar al proceso, se asignan
        // los frames necesarios y se marca el proceso como exitoso. Si no hay 
        // suficientes frames libres, el proceso no se asigna y se marca como no exitoso.
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
            /* Actualizar tabla de procesos con frames asignados */
            if (tabla_idx >= 0) {
                sem_p(semid, SEM_TABLA);
                mem->procesos[tabla_idx].estado = PROC_EN_MEMORIA;
                mem->procesos[tabla_idx].num_frames_asignados = asignados;
                int copied = 0;
                for (int f = 0; f < mem->total_frames && copied < asignados; f++) {
                    if (mem->frames[f].pid == params->pid) {
                        mem->procesos[tabla_idx].frames_asignados[copied++] = f;
                    }
                }
                sem_v(semid, SEM_TABLA);
            }
        }

        
    } else { // SEGMENTACION
        int total_necesario = 0;
        // ciclo para saber por proceso cuantos espacios necesitas
        // num_segmentos = 2,      tam_segmentos = 2
        for (int i = 0; i < params->num_segmentos; i++)
            total_necesario += params->tam_segmentos[i];



        int libres = 0;
        for (int i = 0; i < mem->total_frames; i++)
            if (mem->frames[i].estado == FRAME_LIBRE)
                libres++;

        // se compara cada espacio libre con el tamaño de cada segmento, si el espacio es suficiente para el segmento, se asigna el segmento al proceso y se marca como exitoso. Si no hay suficiente espacio para algún segmento, el proceso no se asigna y se marca como no exitoso.
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
            /* Actualizar tabla de procesos con frames asignados (segmentacion) */
            if (tabla_idx >= 0) {
                sem_p(semid, SEM_TABLA);
                mem->procesos[tabla_idx].estado = PROC_EN_MEMORIA;
                mem->procesos[tabla_idx].num_frames_asignados = asignados;
                int copied = 0;
                for (int f = 0; f < mem->total_frames && copied < asignados; f++) {
                    if (mem->frames[f].pid == params->pid) {
                        mem->procesos[tabla_idx].frames_asignados[copied++] = f;
                    }
                }
                sem_v(semid, SEM_TABLA);
            }
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
        /* Actualizar tabla: terminado y limpiar frames_asignados */
        if (tabla_idx >= 0) {
            sem_p(semid, SEM_TABLA);
            mem->procesos[tabla_idx].estado = PROC_TERMINADO;
            for (int f = 0; f < mem->procesos[tabla_idx].num_frames_asignados; f++)
                mem->procesos[tabla_idx].frames_asignados[f] = -1;
            mem->procesos[tabla_idx].num_frames_asignados = 0;
            sem_v(semid, SEM_TABLA);
        }
        sem_v(semid, SEM_MEMORIA);
        log_mensaje(semid, params->pid, "Liberación de memoria");
    } else {
        log_mensaje(semid, params->pid, "No hay espacio suficiente");
        /* Marcar en tabla como muerto */
        if (tabla_idx >= 0) {
            sem_p(semid, SEM_TABLA);
            mem->procesos[tabla_idx].estado = PROC_MUERTO;
            sem_v(semid, SEM_TABLA);
        }
    }
    pthread_exit(NULL);
}

// main simple para pruebas
int main() {
    crear_procesos();
    return 0;
}
