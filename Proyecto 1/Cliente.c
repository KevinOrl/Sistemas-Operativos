// Cliente: genera procesos y los envia al servidor (thread por proceso)
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int burst;
    int prioridad;
} MensajeProceso;

typedef struct {
    int pid;
    int ok;
} MensajeAck;

typedef struct {
    char host[128];
    int puerto;
    int burst;
    int prioridad;
} HiloProcesoArgs;

volatile sig_atomic_t generarAutomatico = 1;

static void manejarCtrlC(int signal) {
    (void)signal;
    generarAutomatico = 0;
}

static int conectarServidor(const char *host, int puerto) {
    struct hostent *he = gethostbyname(host);
    if (he == NULL) {
        fprintf(stderr, "Error al obtener informacion del host %s\n", host);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("Error al crear el socket");
        return -1;
    }

    struct sockaddr_in servidor;
    memset(&servidor, 0, sizeof(servidor));
    servidor.sin_family = AF_INET;
    servidor.sin_port = htons((unsigned short)puerto);
    servidor.sin_addr = *((struct in_addr *)he->h_addr);

    if (connect(fd, (struct sockaddr *)&servidor, sizeof(servidor)) == -1) {
        perror("Error en connect");
        close(fd);
        return -1;
    }

    return fd;
}

static void *hiloEnviarProceso(void *arg) {
    HiloProcesoArgs *datos = (HiloProcesoArgs *)arg;
    int fd = conectarServidor(datos->host, datos->puerto);

    if (fd == -1) {
        free(datos);
        return NULL;
    }

    MensajeProceso msg;
    msg.burst = datos->burst;
    msg.prioridad = datos->prioridad;

    ssize_t enviados = send(fd, &msg, sizeof(msg), 0);
    if (enviados != (ssize_t)sizeof(msg)) {
        perror("Error enviando proceso");
        close(fd);
        free(datos);
        return NULL;
    }

    MensajeAck ack;
    ssize_t recibidos = recv(fd, &ack, sizeof(ack), 0);
    if (recibidos != (ssize_t)sizeof(ack)) {
        perror("Error recibiendo ACK");
        close(fd);
        free(datos);
        return NULL;
    }

    if (ack.ok) {
        printf("Proceso enviado (Burst=%d, Prioridad=%d) -> PID asignado: %d\n",
               msg.burst,
               msg.prioridad,
               ack.pid);
    } else {
        printf("Servidor rechazo el proceso (Burst=%d, Prioridad=%d)\n", msg.burst, msg.prioridad);
    }

    close(fd);
    free(datos);
    return NULL;
}

static void crearYEsperarHiloProceso(const char *host, int puerto, int burst, int prioridad) {
    HiloProcesoArgs *args = (HiloProcesoArgs *)malloc(sizeof(HiloProcesoArgs));
    if (args == NULL) {
        perror("No se pudo reservar memoria para hilo de proceso");
        return;
    }

    strncpy(args->host, host, sizeof(args->host) - 1);
    args->host[sizeof(args->host) - 1] = '\0';
    args->puerto = puerto;
    args->burst = burst;
    args->prioridad = prioridad;

    pthread_t hiloProceso;
    int estado = pthread_create(&hiloProceso, NULL, hiloEnviarProceso, args);
    if (estado != 0) {
        fprintf(stderr, "Error creando hilo de proceso: %s\n", strerror(estado));
        free(args);
        return;
    }

    pthread_join(hiloProceso, NULL);
}

static void ejecutarModoManual(const char *host, int puerto) {
    FILE *archivo = fopen("Procesos.txt", "r");
    if (archivo == NULL) {
        perror("No se pudo abrir Procesos.txt");
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int burst = 0;
        int prioridad = 0;
        int leidos = sscanf(linea, "%d %d", &burst, &prioridad);

        if (leidos != 2) {
            printf("Linea invalida en archivo: %s", linea);
            continue;
        }

        if (prioridad < 1 || prioridad > 10) {
            printf("Proceso ignorado: prioridad %d fuera de rango [1, 10]\n", prioridad);
            continue;
        }

        crearYEsperarHiloProceso(host, puerto, burst, prioridad);
        sleep((unsigned int)(rand() % 6 + 3));
    }

    fclose(archivo);
}

static void ejecutarModoAutomatico(const char *host, int puerto) {
    int burstMin = 1;
    int burstMax = 20;
    int tasaMin = 1;
    int tasaMax = 5;

    printf("Rango de burst permitido (Automatico)\n");
    printf("Burst minimo: ");
    scanf("%d", &burstMin);
    printf("Burst maximo: ");
    scanf("%d", &burstMax);

    if (burstMin > burstMax) {
        int tmpBurst = burstMin;
        burstMin = burstMax;
        burstMax = tmpBurst;
    }


    generarAutomatico = 1;
    printf("Modo automatico activo. Presione Ctrl+C para detener.\n");

    while (generarAutomatico) {
        int burst = burstMin + rand() % (burstMax - burstMin + 1);
        int prioridad = 1 + rand() % 10;
        int espera = 2 + rand() % 4;

        crearYEsperarHiloProceso(host, puerto, burst, prioridad);

        for (int i = 0; i < espera && generarAutomatico; i++) {
            sleep(1);
        }
    }

    printf("Creacion automatica detenida por usuario.\n");
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Uso: %s <host> <puerto>\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int puerto = atoi(argv[2]);

    srand((unsigned int)time(NULL));
    signal(SIGINT, manejarCtrlC);

    int modo = 0;
    int algoritmo = 0;
    int quantum = 0;

    printf("Seleccione modo cliente:\n");
    printf("1. Manual\n");
    printf("2. Automatico\n");
    scanf("%d", &modo);

    if (modo != 1 && modo != 2) {
        printf("Modo no valido\n");
        return 1;
    }

    printf("Seleccione algoritmo de planificacion:\n");
    printf("1. FIFO\n");    
    printf("2. SJF\n");
    printf("3. HPF\n");
    printf("4. Round Robin\n");
    scanf("%d", &algoritmo);

    switch (algoritmo) {
        case 1:
        case 2:
        case 3:
            break;
        case 4:
            printf("Ingrese el quantum (debe ser mayor a 0): ");
            scanf("%d", &quantum);
            if (quantum <= 0) {
                fprintf(stderr, "Quantum invalido. Debe ser un numero positivo.\n");
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Algoritmo no valido. Use 1, 2, 3 o 4.\n");
            return 1;
    }

    // Enviar algoritmo y quantum al servidor antes de enviar procesos
    int fd_alg = conectarServidor(host, puerto);
    if (fd_alg == -1) {
        fprintf(stderr, "No se pudo conectar al servidor para enviar el algoritmo y quantum.\n");
        return 1;
    }
    int datos[2];
    datos[0] = algoritmo;
    datos[1] = quantum;
    ssize_t enviados_alg = send(fd_alg, datos, sizeof(datos), 0);
    if (enviados_alg != sizeof(datos)) {
        perror("Error enviando algoritmo y quantum al servidor");
        close(fd_alg);
        return 1;
    }
    close(fd_alg);

    switch (modo) {
        case 1:
            ejecutarModoManual(host, puerto);
            break;
        case 2:
            ejecutarModoAutomatico(host, puerto);
            break;
        default:
            printf("Modo no valido\n");
            return 1;
    }

    return 0;
}