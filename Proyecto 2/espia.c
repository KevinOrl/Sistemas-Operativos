/*
espia.c

Permite consultar el estado de la memoria, los procesos y la bitácora.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include "common.h"
#include "espia.h"

void mostrar_estado_memoria(MemoriaCompartida *mem) {
    printf("\n--- Estado de la Memoria ---\n");
    printf("Frame\tEstado\tPID\tSegmento\n");
    for (int i = 0; i < mem->total_frames; i++) {
        printf("%d\t%s\t%d\t%d\n", i,
            mem->frames[i].estado == FRAME_LIBRE ? "LIBRE" : "OCUPADO",
            mem->frames[i].pid,
            mem->frames[i].segmento);
    }
}

void mostrar_estado_procesos(MemoriaCompartida *mem) {
    printf("\n--- Estado de los Procesos ---\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        InfoProceso *p = &mem->procesos[i];
        if (p->estado == PROC_VACIO) continue;
        printf("PID: %d\tEstado: ", p->pid);
        switch (p->estado) {
            case PROC_BUSCANDO:   printf("BUSCANDO ESPACIO\n"); break;
            case PROC_EN_MEMORIA: printf("EN MEMORIA (durmiendo)\n"); break;
            case PROC_BLOQUEADO:  printf("BLOQUEADO\n"); break;
            case PROC_MUERTO:     printf("MUERTO (sin espacio)\n"); break;
            case PROC_TERMINADO:  printf("TERMINADO\n"); break;
            default:              printf("DESCONOCIDO\n"); break;
        }
    }
}

void mostrar_bitacora() {
    printf("\n--- Bitácora de eventos ---\n");
    FILE *f = fopen(LOG_FILE, "r");
    if (!f) { perror("No se pudo abrir la bitácora"); return; }
    char linea[256];
    while (fgets(linea, sizeof(linea), f)) {
        printf("%s", linea);
    }
    fclose(f);
}

void menu_espia() {
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);
    if (shmid < 0) { perror("shmget"); exit(1); }
    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1) { perror("shmat"); exit(1); }
    int opcion;
    do {
        printf("\n===== MENÚ ESPIA =====\n");
        printf("1) Ver estado de la memoria\n");
        printf("2) Ver estado de los procesos\n");
        printf("3) Ver bitácora\n");
        printf("4) Salir\n");
        printf("Opción: ");
        scanf("%d", &opcion);
        switch (opcion) {
            case 1: mostrar_estado_memoria(mem); break;
            case 2: mostrar_estado_procesos(mem); break;
            case 3: mostrar_bitacora(); break;
            case 4: printf("Saliendo...\n"); break;
            default: printf("Opción inválida.\n"); break;
        }
    } while (opcion != 4);
    shmdt(mem);
}

int main() {
    menu_espia();
    return 0;
}
