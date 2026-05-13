/*
finalizador.c

Libera todos los recursos IPC, mata procesos activos y registra el cierre en la bitácora.
*/

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include "common.h"
#include "finalizador.h"

void finalizar_sistema() {
    // 1. Conectarse a memoria compartida y semáforos
    int shmid = shmget(SHM_KEY, sizeof(MemoriaCompartida), 0666);
    if (shmid < 0) { perror("shmget"); return; }
    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1) { perror("shmat"); return; }
    int semid = semget(SEM_KEY, NUM_SEMS, 0666);
    if (semid < 0) { perror("semget"); return; }

    // 2. Tomar semáforo de memoria para sincronizar
    sem_p(semid, SEM_MEMORIA);

    // 3. Matar procesos activos y limpiar tabla
    printf("Matando procesos activos...\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        InfoProceso *p = &mem->procesos[i];
        if (p->estado != PROC_VACIO && p->estado != PROC_MUERTO && p->estado != PROC_TERMINADO) {
            if (p->pid > 0) {
                kill(p->pid, SIGTERM);
                printf("Enviado SIGTERM a PID %d\n", p->pid);
                log_mensaje(semid, p->pid, "Finalizado por el finalizador");
            }
        }
    }
    sleep(1); // Dar tiempo a que terminen

    // Limpiar tabla de procesos y frames
    for (int i = 0; i < MAX_PROCESSES; i++) {
        mem->procesos[i].estado = PROC_VACIO;
        mem->procesos[i].pid = -1;
        mem->procesos[i].num_frames_asignados = 0;
    }
    for (int i = 0; i < mem->total_frames; i++) {
        mem->frames[i].estado = FRAME_LIBRE;
        mem->frames[i].pid = -1;
        mem->frames[i].segmento = -1;
    }

    // 4. Liberar semáforo de memoria
    sem_v(semid, SEM_MEMORIA);

    // 5. Liberar recursos IPC
    printf("Liberando memoria compartida y semáforos...\n");
    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);

    // 6. Registrar cierre en bitácora
    FILE *f = fopen(LOG_FILE, "a");
    if (f) {
        fprintf(f, "\n[SISTEMA FINALIZADO] Todos los recursos liberados.\n");
        fclose(f);
    }
    printf("Sistema finalizado y recursos liberados.\n");
}

int main() {
    finalizar_sistema();
    return 0;
}
