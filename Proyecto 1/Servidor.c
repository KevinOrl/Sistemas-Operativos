// Servidor: Job Scheduler + CPU Scheduler
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>

typedef struct {
    int burst;
    int prioridad;
} MensajeProceso;

typedef struct {
    int pid;
    int ok;
} MensajeAck;

typedef struct PCB {
    int pid;
    int burstTotal;
    int burstRestante;
    int prioridad;
    int llegada;
    int salida;
    int tat;
    int wt;
    struct PCB *next;
} PCB;

typedef struct Finalizado {
    int pid;
    int llegada;
    int burst;
    int salida;
    int tat;
    int wt;
    struct Finalizado *next;
} Finalizado;

static PCB *readyHead = NULL;
static Finalizado *finHead = NULL;

static pthread_mutex_t readyMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t readyCond = PTHREAD_COND_INITIALIZER;

static volatile sig_atomic_t servidorActivo = 1;

static int listenFd = -1;
static int nextPid = 1;
static time_t startTime;

static int procesosEjecutados = 0;
static int cpuOciosoSegundos = 0;

static int tiempoRelativoAhora(void) {
    return (int)(time(NULL) - startTime);
}

static void registrarFinalizado(const PCB *p) {
    Finalizado *n = (Finalizado *)malloc(sizeof(Finalizado));
    if (n == NULL) {
        perror("No se pudo registrar finalizado");
        return;
    }

    n->pid = p->pid;
    n->llegada = p->llegada;
    n->burst = p->burstTotal;
    n->salida = p->salida;
    n->tat = p->tat;
    n->wt = p->wt;
    n->next = finHead;
    finHead = n;
}

static void enqueueReady(PCB *p) {
    p->next = NULL;
    if (readyHead == NULL) {
        readyHead = p;
        return;
    }

    PCB *cur = readyHead;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = p;
}

static PCB *dequeueFIFO(void) {
    if (readyHead == NULL) {
        return NULL;
    }
    PCB *p = readyHead;
    readyHead = readyHead->next;
    p->next = NULL;
    return p;
}

static PCB *seleccionarSiguiente(void) {
    return dequeueFIFO();
}

static void imprimirReady(void) {
    pthread_mutex_lock(&readyMutex);

    printf("\n[READY] Estado actual de la lista:\n");
    if (readyHead == NULL) {
        printf("(vacia)\n");
    } else {
        PCB *cur = readyHead;
        while (cur != NULL) {
            printf("PID=%d BurstRestante=%d Prioridad=%d Llegada=%d\n",
                   cur->pid,
                   cur->burstRestante,
                   cur->prioridad,
                   cur->llegada);
            cur = cur->next;
        }
    }

    pthread_mutex_unlock(&readyMutex);
}

static void *jobSchedulerThread(void *arg) {
    (void)arg;

    while (servidorActivo) {
        struct sockaddr_in cliente;
        socklen_t lenCliente = sizeof(cliente);
        int fd2 = accept(listenFd, (struct sockaddr *)&cliente, &lenCliente);

        if (fd2 == -1) {
            if (!servidorActivo) {
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            perror("Error en accept");
            continue;
        }

        MensajeProceso msg;
        ssize_t r = recv(fd2, &msg, sizeof(msg), 0);
        if (r == (ssize_t)sizeof(msg)) {
            PCB *p = (PCB *)malloc(sizeof(PCB));
            if (p != NULL) {
                p->pid = __sync_fetch_and_add(&nextPid, 1);
                p->burstTotal = msg.burst;
                p->burstRestante = msg.burst;
                p->prioridad = msg.prioridad;
                p->llegada = tiempoRelativoAhora();
                p->salida = 0;
                p->tat = 0;
                p->wt = 0;
                p->next = NULL;

                pthread_mutex_lock(&readyMutex);
                enqueueReady(p);
                pthread_cond_signal(&readyCond);
                pthread_mutex_unlock(&readyMutex);

                MensajeAck ack;
                ack.pid = p->pid;
                ack.ok = 1;
                send(fd2, &ack, sizeof(ack), 0);
            } else {
                MensajeAck ack;
                ack.pid = -1;
                ack.ok = 0;
                send(fd2, &ack, sizeof(ack), 0);
            }
        }

        close(fd2);
    }

    return NULL;
}

static void ejecutarProceso(PCB *p) {
    if (p == NULL) {
        return;
    }

    int cuanto = p->burstRestante;

    printf("Proceso PID=%d entra en ejecucion (Burst=%d, Prioridad=%d)\n",
           p->pid,
           p->burstRestante,
           p->prioridad);

    sleep((unsigned int)cuanto);
    p->burstRestante -= cuanto;

    if (p->burstRestante <= 0) {
        p->salida = tiempoRelativoAhora();
        p->tat = p->salida - p->llegada;
        p->wt = p->tat - p->burstTotal;

        printf("Proceso PID=%d finalizado. Salida=%d TAT=%d WT=%d\n",
               p->pid,
               p->salida,
               p->tat,
               p->wt);

        procesosEjecutados++;
        registrarFinalizado(p);
        free(p);
    } else {
        pthread_mutex_lock(&readyMutex);
        enqueueReady(p);
        pthread_cond_signal(&readyCond);
        pthread_mutex_unlock(&readyMutex);
    }
}

static void *cpuSchedulerThread(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&readyMutex);

        while (readyHead == NULL && servidorActivo) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;

            int rc = pthread_cond_timedwait(&readyCond, &readyMutex, &ts);
            if (rc == ETIMEDOUT && readyHead == NULL && servidorActivo) {
                cpuOciosoSegundos++;
            }
        }

        if (!servidorActivo && readyHead == NULL) {
            pthread_mutex_unlock(&readyMutex);
            break;
        }

        PCB *p = seleccionarSiguiente();
        pthread_mutex_unlock(&readyMutex);

        ejecutarProceso(p);
    }

    return NULL;
}

static void liberarReady(void) {
    PCB *cur = readyHead;
    while (cur != NULL) {
        PCB *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    readyHead = NULL;
}

static void liberarFinalizados(void) {
    Finalizado *cur = finHead;
    while (cur != NULL) {
        Finalizado *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    finHead = NULL;
}

static void imprimirReporteFinal(void) {
    double sumaWT = 0.0;
    int count = 0;

    printf("\n===== REPORTE FINAL SERVIDOR =====\n");
    printf("Procesos ejecutados: %d\n", procesosEjecutados);
    printf("CPU ocioso (segundos): %d\n", cpuOciosoSegundos);

    printf("\nTabla TAT/WT:\n");
    printf("PID\tLlegada\tBurst\tSalida\tTAT\tWT\n");

    Finalizado *cur = finHead;
    while (cur != NULL) {
        printf("%d\t%d\t%d\t%d\t%d\t%d\n",
               cur->pid,
               cur->llegada,
               cur->burst,
               cur->salida,
               cur->tat,
               cur->wt);
        sumaWT += cur->wt;
        count++;
        cur = cur->next;
    }

    printf("Promedio WT: %.2f\n", (count > 0) ? (sumaWT / count) : 0.0);
}

static void manejarSigint(int signal) {
    (void)signal;
    servidorActivo = 0;
    if (listenFd != -1) {
        close(listenFd);
        listenFd = -1;
    }
    pthread_cond_broadcast(&readyCond);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int puerto = atoi(argv[1]);
    startTime = time(NULL);
    signal(SIGINT, manejarSigint);

    printf("Servidor iniciado en modo FIFO para pruebas iniciales.\n");

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        perror("Error al crear socket servidor");
        return 1;
    }

    int opt = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servidor;
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons((unsigned short)puerto);
    servidor.sin_addr.s_addr = INADDR_ANY;

    if (bind(listenFd, (struct sockaddr *)&servidor, sizeof(servidor)) == -1) {
        perror("Error en bind");
        close(listenFd);
        return 1;
    }

    if (listen(listenFd, 10) == -1) {
        perror("Error en listen");
        close(listenFd);
        return 1;
    }

    pthread_t jobThread;
    pthread_t cpuThread;

    if (pthread_create(&jobThread, NULL, jobSchedulerThread, NULL) != 0) {
        perror("Error creando Job Scheduler");
        close(listenFd);
        return 1;
    }

    if (pthread_create(&cpuThread, NULL, cpuSchedulerThread, NULL) != 0) {
        perror("Error creando CPU Scheduler");
        servidorActivo = 0;
        close(listenFd);
        pthread_cond_broadcast(&readyCond);
        pthread_join(jobThread, NULL);
        return 1;
    }

    printf("Servidor activo en puerto %d. Comandos: cola | salir\n", puerto);

    char comando[64];
    while (servidorActivo && scanf("%63s", comando) == 1) {
        if (strcmp(comando, "cola") == 0) {
            imprimirReady();
        } else if (strcmp(comando, "salir") == 0) {
            manejarSigint(SIGINT);
            break;
        } else {
            printf("Comando no reconocido. Use: cola | salir\n");
        }
    }

    servidorActivo = 0;
    if (listenFd != -1) {
        close(listenFd);
        listenFd = -1;
    }
    pthread_cond_broadcast(&readyCond);

    pthread_join(jobThread, NULL);
    pthread_join(cpuThread, NULL);

    imprimirReporteFinal();
    liberarReady();
    liberarFinalizados();

    return 0;
}