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
#include <sys/select.h>
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
static int algoritmo = 0;
static int quantum = 0; // Para Round Robin

static pthread_mutex_t readyMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t readyCond = PTHREAD_COND_INITIALIZER;

static volatile sig_atomic_t servidorActivo = 1;

static int listenFd = -1;
static int nextPid = 1;
static time_t tiempoBaseSimulacion = 0;
static int baseSimulacionInicializada = 0;

static int procesosEjecutados = 0;
static int cpuOciosoSegundos = 0;

static const char *nombreAlgoritmo(int alg) {
    switch (alg) {
        case 1:
            return "FIFO";
        case 2:
            return "SJF";
        case 3:
            return "HPF";
        case 4:
            return "RR";
        default:
            return "DESCONOCIDO";
    }
}

static void detenerServidor(void) {
    servidorActivo = 0;

    if (listenFd != -1) {
        shutdown(listenFd, SHUT_RDWR);
        close(listenFd);
        listenFd = -1;
    }

    pthread_cond_broadcast(&readyCond);
}

static int tiempoRelativoAhora(void) {
    if (!baseSimulacionInicializada) {
        return 0;
    }
    return (int)(time(NULL) - tiempoBaseSimulacion);
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
/*Acomoda los procesos que van llegando a la lista de ready*/
static void enqueueReady(PCB *p) {
    p->next = NULL;

    /*Si no hay proceso actual ejecutandose entonces el 
    que entra se coloca como cabecilla*/

    if (readyHead == NULL) {
        readyHead = p;
        return;
    }

    /*Si hay mas procesos en cola, entonces se recorre cada uno de los siguientes de cada proceso
    para llegar al final de la lista y asignar el que acaba de llegar*/

    PCB *cur = readyHead;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    cur->next = p;
}

/*Saca el primer proceso de la cola READY (orden FIFO real de cola).*/
static PCB *dequeueReady(void) {
    if (readyHead == NULL) {
        return NULL;
    }
    PCB *p = readyHead;
    readyHead = readyHead->next;
    p->next = NULL;
    return p;
}

/*FIFO por criterio de PID: selecciona el menor PID disponible en READY.*/
static PCB *seleccionarSiguienteFIFOporPID(void) {
    if (readyHead == NULL) {
        return NULL;
    }

    PCB *prevMin = NULL;
    PCB *min = readyHead;
    PCB *prev = readyHead;
    PCB *cur = readyHead->next;

    while (cur != NULL) {
        if (cur->pid < min->pid) {
            prevMin = prev;
            min = cur;
        }
        prev = cur;
        cur = cur->next;
    }

    if (min == readyHead) {
        readyHead = min->next;
    } else if (prevMin != NULL) {
        prevMin->next = min->next;
    }
    min->next = NULL;
    return min;
}

// Selecciona el proceso con mayor prioridad (menor valor de prioridad)
// En caso de empate, selecciona el que llegó primero (FIFO entre los de misma prioridad)
// Esta es similar al de SFJ, pero con prioridad en lugar de burst restante
static PCB *seleccionarSiguienteHPF(void) {
    if (readyHead == NULL) {
        return NULL;
    }
    PCB *prevMax = NULL;
    PCB *max = readyHead;
    PCB *prev = readyHead;
    PCB *cur = readyHead->next;
    while (cur != NULL) {
        if (cur->prioridad < max->prioridad) {
            prevMax = prev;
            max = cur;
        }
        prev = cur;
        cur = cur->next;
    }
    // Quitar max de la lista
    if (max == readyHead) {
        readyHead = max->next;
    } else if (prevMax != NULL) {
        prevMax->next = max->next;
    }
    max->next = NULL;
    return max;
}

static PCB *seleccionarSiguienteSJF(void) {
    if (readyHead == NULL) {
        return NULL;
    }
    /*Se define 4 PCBs para ir recorriendo y buscando el proceso con 
    el menor burst restante*/
    PCB *prevMin = NULL;
    PCB *min = readyHead;
    PCB *prev = readyHead;
    PCB *cur = readyHead->next;

    /*Se recorre la lista de procesos listos (readyHead)*/
    while (cur != NULL) {
        if (cur->burstRestante < min->burstRestante) {
            prevMin = prev;
            min = cur;
        }
        prev = cur;
        cur = cur->next;
    }
    // Quitar min de la lista
    if (min == readyHead) {
        readyHead = min->next;
    } else if (prevMin != NULL) {
        prevMin->next = min->next;
    }
    min->next = NULL;
    /*Se retorna el proceso seleccionado*/
    return min;
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
            if (!baseSimulacionInicializada) {
                tiempoBaseSimulacion = time(NULL);
                baseSimulacionInicializada = 1;
            }

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

static void ejecutarProceso(PCB *p, int quantum, int algoritmoEjecucion) {
    if (p == NULL) {
        return;
    }

    int tiempoEntradaCPU = tiempoRelativoAhora();
    const char *algNombre = nombreAlgoritmo(algoritmoEjecucion);

    if (quantum > 0)
    {
        int burstAntes = p->burstRestante;
        int cuanto = (quantum > burstAntes) ? burstAntes : quantum;
         printf("[%s] PID=%d entra en ejecucion. Burst restante=%d, Quantum=%d\n",
             algNombre,
               p->pid,
               burstAntes,
               quantum);
         printf("[%s] PID=%d tramo inicia en t=%d y ejecuta %d segundos\n",
             algNombre,
               p->pid,
               tiempoEntradaCPU,
               cuanto);

        //Validacion para saber si el quantum es mayor al burst restante, 
        //si es así se duerme el tiempo del burst restante y se asigna como 0
        if (quantum > p->burstRestante)
        {
            sleep((unsigned int)p->burstRestante);
            p->burstRestante = 0;

        }else{
            sleep((unsigned int)quantum);
            p->burstRestante -= quantum;
        }

        printf("[%s] PID=%d tramo termina en t=%d\n", algNombre, p->pid, tiempoRelativoAhora());

    
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
                 printf("[%s] PID=%d agoto quantum. Burst restante ahora=%d. Reingresa a READY.\n",
                     algNombre,
                   p->pid,
                   p->burstRestante);
            pthread_mutex_lock(&readyMutex);
            enqueueReady(p);
            pthread_cond_signal(&readyCond);
            pthread_mutex_unlock(&readyMutex);
        }
    }else{
        int cuanto = p->burstRestante;
        printf("[%s] PID=%d entra en ejecucion (Llegada=%d, Burst=%d, Prioridad=%d)\n",
            algNombre,
            p->pid,
            p->llegada,
            p->burstRestante,
            p->prioridad);
        printf("[%s] PID=%d tramo inicia en t=%d y ejecuta %d segundos\n",
               algNombre,
               p->pid,
               tiempoEntradaCPU,
               cuanto);

        sleep((unsigned int)cuanto);
        p->burstRestante -= cuanto;

        printf("[%s] PID=%d tramo termina en t=%d\n", algNombre, p->pid, tiempoRelativoAhora());

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
   

}

static void *cpuSchedulerThread(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&readyMutex);
        /*Esta parte toma el tiempo actual (segundos y nanosegundos) y le suma un segundo
        posteriormente, va con pthread_cond_timedwait dice que si algun otro hilo cambia o
        el tiempo de un segundo pasó, entonces se le asigna como 0 si otro hilo esta listo
        o ETIMEOUT si se paso el tiempo*/
        while (readyHead == NULL && servidorActivo) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            
            /*Se pregunta si se venció el tiempo de un segundo para sumar ese segundo como
            cpu ocioso*/
            int rc = pthread_cond_timedwait(&readyCond, &readyMutex, &ts);
            if (rc == ETIMEDOUT && readyHead == NULL && servidorActivo) {
                cpuOciosoSegundos++;
            }
        }

        if (!servidorActivo && readyHead == NULL) {
            pthread_mutex_unlock(&readyMutex);
            break;
        }

        /*
        Esta funcion funciona para FIFO
        Deberia haber mas funciones para cada algoritmo
        porque esta solamente trae el primer proceso que entró (FIFO)
        */

        PCB *p = NULL;
        int quantumEjecucion = 0;

        switch (algoritmo) {
            case 1:
                p = seleccionarSiguienteFIFOporPID();
                quantumEjecucion = 0;
                break;
            case 2:
                p = seleccionarSiguienteSJF();
                quantumEjecucion = 0;
                break;
            case 3:
                p = seleccionarSiguienteHPF();
                quantumEjecucion = 0;
                break;
            case 4:
                p = dequeueReady();
                quantumEjecucion = quantum;
                break;
            default:
                pthread_mutex_unlock(&readyMutex);
                fprintf(stderr, "Algoritmo no valido: %d\n", algoritmo);
                continue;
        }

        pthread_mutex_unlock(&readyMutex);
        ejecutarProceso(p, quantumEjecucion, algoritmo);
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
    detenerServidor();
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int puerto = atoi(argv[1]);
    signal(SIGINT, manejarSigint);

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


    /* Recibe el tipo de algoritmo y quantum a utilizar */
    int fd_alg = accept(listenFd, NULL, NULL);
    if (fd_alg == -1) {
        perror("Error en accept para algoritmo");
        close(listenFd);
        return 1;
    }
    /*Saca los datos del algoritmo y quantum */
    int datos[2] = {0, 0};
    ssize_t r_alg = recv(fd_alg, datos, sizeof(datos), 0);
    if (r_alg != sizeof(datos)) {
        perror("Error recibiendo algoritmo y quantum");
        close(fd_alg);
        close(listenFd);
        return 1;
    }
    /*Asigna los valores recibidos en las variables*/
    algoritmo = datos[0];
    quantum = datos[1];
    close(fd_alg);

    if (algoritmo < 1 || algoritmo > 4) {
        fprintf(stderr, "Algoritmo invalido recibido: %d\n", algoritmo);
        close(listenFd);
        return 1;
    }

    if (algoritmo == 4 && quantum <= 0) {
        fprintf(stderr, "Quantum invalido recibido para RR: %d\n", quantum);
        close(listenFd);
        return 1;
    }

    printf("Algoritmo de planificacion recibido: %d\n", algoritmo);
    if (algoritmo == 4) {
        printf("Quantum recibido: %d\n", quantum);
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
    while (servidorActivo) {
        fd_set lectura;
        struct timeval timeout;

        FD_ZERO(&lectura);
        FD_SET(STDIN_FILENO, &lectura);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int listos = select(STDIN_FILENO + 1, &lectura, NULL, NULL, &timeout);
        if (listos < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("Error en select de comandos");
            break;
        }

        if (listos == 0) {
            continue;
        }

        if (FD_ISSET(STDIN_FILENO, &lectura)) {
            if (scanf("%63s", comando) != 1) {
                if (feof(stdin)) {
                    clearerr(stdin);
                }
                continue;
            }

            if (strcmp(comando, "cola") == 0) {
                imprimirReady();
            } else if (strcmp(comando, "salir") == 0) {
                detenerServidor();
                break;
            } else {
                printf("Comando no reconocido. Use: cola | salir\n");
            }
        }
    }

    detenerServidor();

    pthread_join(jobThread, NULL);
    pthread_join(cpuThread, NULL);

    imprimirReporteFinal();
    liberarReady();
    liberarFinalizados();

    return 0;
}