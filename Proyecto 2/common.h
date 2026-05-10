#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>
#include <stdio.h>

// CLAVES IPC  (deben ser iguales en todos los programas)
#define SHM_KEY   0x4321       // clave para shmget
#define SEM_KEY   0x8765       // lave para semget
#define LOG_FILE  "bitacora.txt"


// LÍMITES DEL SISTEMA
#define MAX_FRAMES          100   // máximo de frames físicos
#define MAX_PROCESSES        50   // máximo de procesos
#define MAX_SEGS              5   // máximo segmentos/proceso
#define MAX_FRAMES_POR_PROC  30   // 10 páginas ó 5seg×3frames


// ÍNDICES DE SEMÁFOROS  (un semget con 3 semáforos)
#define SEM_MEMORIA  0    //  región crítica búsq/liberación
#define SEM_LOG      1    // región crítica escritura log
#define SEM_TABLA    2    // región crítica tabla procesos
#define NUM_SEMS     3


// ENUMERACIONES
typedef enum { PAGINACION = 0, SEGMENTACION = 1 } Esquema;

typedef enum { FRAME_LIBRE = 0, FRAME_OCUPADO = 1 } EstadoFrame;

typedef enum {
    PROC_BUSCANDO  = 0,   // buscando espacio en memoria
    PROC_EN_MEMORIA = 1,  // en memoria, durmiendo
    PROC_BLOQUEADO  = 2,  // esperando semáforo
    PROC_MUERTO     = 3,  // murió por falta de espacio
    PROC_TERMINADO  = 4,  // terminó su ejecución normal
    PROC_VACIO      = 5   // entrada libre en la tabla
} EstadoProceso;


// ESTRUCTURAS DE DATOS EN MEMORIA COMPARTIDA
// Un frame físico de memoria
typedef struct {
    EstadoFrame estado;
    pid_t       pid;        // proceso dueño (-1 si libre)
    int         segmento;   // índice de segmento (-1 si paginación)
} Frame;

// Información de un proceso en el sistema
typedef struct {
    pid_t        pid;
    EstadoProceso estado;
    int          tiempo;                        // segundos de sleep
    // Paginación
    int          num_paginas;
    // Segmentación
    int          num_segmentos;
    int          tam_segmentos[MAX_SEGS];        // tamaño de cada segmento
    // Frames asignados (aplica para ambos esquemas)
    int          frames_asignados[MAX_FRAMES_POR_PROC];
    int          num_frames_asignados;
} InfoProceso;

// Segmento de memoria compartida completo
typedef struct {
    Esquema     esquema;
    int         total_frames;
    Frame       frames[MAX_FRAMES];
    int         num_procesos;                    // entradas usadas en tabla    
    InfoProceso procesos[MAX_PROCESSES];
} MemoriaCompartida;


// UNION REQUERIDA POR semctl
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};


// FUNCIONES INLINE DE SEMÁFOROS
// P(s) — wait — tomar el semáforo
static inline void sem_p(int semid, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, -1, 0 };
    semop(semid, &op, 1);
}

// V(s) — signal — liberar el semáforo
static inline void sem_v(int semid, int sem_num) {
    struct sembuf op = { (unsigned short)sem_num, 1, 0 };
    semop(semid, &op, 1);
}


// HELPERS DE LOG  (protegidos externamente con SEM_LOG)
static inline void log_evento(int semid, const char *accion,
                               const char *tipo, pid_t pid,
                               int frame, int segmento) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char hora[32];
    strftime(hora, sizeof(hora), "%Y-%m-%d %H:%M:%S", t);

    sem_p(semid, SEM_LOG);
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        if (segmento >= 0)
            fprintf(f, "[%s] PID=%d | %s | Tipo=%-14s | Frame=%3d | Segmento=%d\n",
                    hora, pid, accion, tipo, frame, segmento);
        else
            fprintf(f, "[%s] PID=%d | %s | Tipo=%-14s | Frame=%3d\n",
                    hora, pid, accion, tipo, frame);
        fclose(f);
    }
    sem_v(semid, SEM_LOG);
}

static inline void log_mensaje(int semid, pid_t pid, const char *msg) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char hora[32];
    strftime(hora, sizeof(hora), "%Y-%m-%d %H:%M:%S", t);

    sem_p(semid, SEM_LOG);
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "[%s] PID=%d | %s\n", hora, pid, msg);
        fclose(f);
    }
    sem_v(semid, SEM_LOG);
}

#endif