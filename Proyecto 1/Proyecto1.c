#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include<unistd.h>
#include <time.h>

struct algoritmos {
    int id;
    char nombre[28];
};

struct algoritmos listaAlgoritmos[] = {
    {1, "First-In First-Out (FIFO)"},
    {2, "Shortest Job First (SJF)"},
    {3, "Highest Priority First (HPF)"},
    {4, "Round Robin (RR)"}
};

void mostrarAlgoritmos(struct algoritmos algoritmos[], int cantidad){
    for (int i = 0; i < cantidad; i++) {
        printf("%d. %s\n", algoritmos[i].id, algoritmos[i].nombre);
    }
}

struct Proceso {
    int PID;
    int burst;
    int prioridad;
    int llegada;
    int burstRestante;
    int estado; // 0: listo, 1: ejecutando, 2: terminado
    int salida;
    int tat;
    int wt;
};

struct PCB {
    struct Proceso proceso;
    struct PCB *siguiente;
};

struct PCB *listaProcesos = NULL;
int siguientePID = 1;

struct PCB *crearNodoProceso(int burst, int prioridad, int llegada) {
    struct PCB *nuevo = (struct PCB *)malloc(sizeof(struct PCB));
    if (nuevo == NULL) {
        perror("No se pudo reservar memoria para PCB");
        return NULL;
    }

    nuevo->proceso.PID = siguientePID++;
    nuevo->proceso.burst = burst;
    nuevo->proceso.prioridad = prioridad;
    nuevo->proceso.llegada = llegada;
    nuevo->proceso.burstRestante = burst;
    nuevo->proceso.estado = 0;
    nuevo->proceso.salida = 0;
    nuevo->proceso.tat = 0;
    nuevo->proceso.wt = 0;
    nuevo->siguiente = NULL;

    return nuevo;
}

void insertarOrdenLlegada(struct PCB **lista, struct PCB *nuevo) {
    if (*lista == NULL || nuevo->proceso.llegada < (*lista)->proceso.llegada) {
        nuevo->siguiente = *lista;
        *lista = nuevo;
        return;
    }

    struct PCB *actual = *lista;
    while (actual->siguiente != NULL &&
           actual->siguiente->proceso.llegada <= nuevo->proceso.llegada) {
        actual = actual->siguiente;
    }

    nuevo->siguiente = actual->siguiente;
    actual->siguiente = nuevo;
}

void imprimirLista(struct PCB *lista) {
    if (lista == NULL) {
        printf("No hay procesos cargados.\n");
        return;
    }

    printf("\nProcesos cargados en lista (orden llegada):\n");
    while (lista != NULL) {
        printf("PID=%d Llegada=%d Burst=%d Prioridad=%d\n",
               lista->proceso.PID,
               lista->proceso.llegada,
               lista->proceso.burst,
               lista->proceso.prioridad);
        lista = lista->siguiente;
    }
}

void ejecutarFIFO(struct PCB *lista) {
    int tiempoActual = 0;
    double sumaWT = 0.0;
    int cantidad = 0;

    if (lista == NULL) {
        printf("No hay procesos para ejecutar en FIFO.\n");
        return;
    }

    printf("\n=== Ejecucion FIFO ===\n");
    while (lista != NULL) {
        if (tiempoActual < lista->proceso.llegada) {
            tiempoActual = lista->proceso.llegada;
        }

        lista->proceso.estado = 1;
        tiempoActual += lista->proceso.burst;

        lista->proceso.salida = tiempoActual;
        lista->proceso.tat = lista->proceso.salida - lista->proceso.llegada;
        lista->proceso.wt = lista->proceso.tat - lista->proceso.burst;
        lista->proceso.burstRestante = 0;
        lista->proceso.estado = 2;

        printf("PID=%d | Llegada=%d | Burst=%d | Salida=%d | TAT=%d | WT=%d\n",
               lista->proceso.PID,
               lista->proceso.llegada,
               lista->proceso.burst,
               lista->proceso.salida,
               lista->proceso.tat,
               lista->proceso.wt);

        sumaWT += lista->proceso.wt;
        cantidad++;
        lista = lista->siguiente;
    }

    printf("Promedio WT: %.2f\n", (cantidad > 0) ? (sumaWT / cantidad) : 0.0);
}

void liberarLista(struct PCB *lista) {
    struct PCB *tmp;
    while (lista != NULL) {
        tmp = lista;
        lista = lista->siguiente;
        free(tmp);
    }
}

void *saludoHilo(void *arg) {
    printf("Hola desde el hilo!\n");
    return NULL;
}

void lecturaManual(int algoritmo){
    FILE *archivo = fopen("Procesos.txt", "r");
    if (archivo == NULL) {
        perror("No se pudo abrir Procesos.txt");
        return;
    }

    char linea[256];
    int tiempoLlegadaActual = 0;

    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int llegada = 0;
        int burst = 0;
        int prioridad = 0;
        int pausa = rand() % 6 + 3;
        bool procesoValido = false;
        int leidos = sscanf(linea, "%d %d %d", &llegada, &burst, &prioridad);

        if (leidos == 3) {
            struct PCB *nuevo = crearNodoProceso(burst, prioridad, llegada);
            if (nuevo != NULL) {
                insertarOrdenLlegada(&listaProcesos, nuevo);
                procesoValido = true;
            }
        } else if (sscanf(linea, "%d %d", &burst, &prioridad) == 2) {
            struct PCB *nuevo = crearNodoProceso(burst, prioridad, tiempoLlegadaActual);
            if (nuevo != NULL) {
                insertarOrdenLlegada(&listaProcesos, nuevo);
                procesoValido = true;
            }
            tiempoLlegadaActual += pausa;
        } else {
            printf("Linea invalida: %s", linea);
        }

        if (procesoValido) {
            sleep(pausa);
        }
    }

    fclose(archivo);

    imprimirLista(listaProcesos);

    if (algoritmo == 1) {
        ejecutarFIFO(listaProcesos);
    } else {
        printf("Algoritmo aun no implementado en esta version.\n");
    }
}

void lecturaAutomatica(int algoritmo){
    int burstMin, burstMax;
    int cantidadProcesos;
    int llegadaMin, llegadaMax;
    int tiempoLlegadaActual = 0;

    printf("Ingrese el burst minimo: ");
    scanf("%d", &burstMin);
    printf("Ingrese el burst maximo: ");
    scanf("%d", &burstMax);

    printf("Ingrese la cantidad de procesos a generar: ");
    scanf("%d", &cantidadProcesos);

    printf("Ingrese llegada minima entre procesos: ");
    scanf("%d", &llegadaMin);
    printf("Ingrese llegada maxima entre procesos: ");
    scanf("%d", &llegadaMax);

    if (burstMin > burstMax) {
        int temporal = burstMin;
        burstMin = burstMax;
        burstMax = temporal;
    }

    if (llegadaMin > llegadaMax) {
        int temporal = llegadaMin;
        llegadaMin = llegadaMax;
        llegadaMax = temporal;
    }

    printf("Modo automatico iniciado.\n");

    for (int i = 0; i < cantidadProcesos; i++) {
        int burst = burstMin + rand() % (burstMax - burstMin + 1);
        int prioridad = 1 + rand() % 10;
        int intervaloLlegada = llegadaMin + rand() % (llegadaMax - llegadaMin + 1);

        struct PCB *nuevo = crearNodoProceso(burst, prioridad, tiempoLlegadaActual);
        if (nuevo != NULL) {
            insertarOrdenLlegada(&listaProcesos, nuevo);
        }

        tiempoLlegadaActual += intervaloLlegada;
    }

    imprimirLista(listaProcesos);

    if (algoritmo == 1) {
        ejecutarFIFO(listaProcesos);
    } else {
        printf("Algoritmo aun no implementado en esta version.\n");
    }
}


int main() {
    pthread_t hilo;
    srand(time(NULL));

    //primer argumento es nuestro hilo referenciado, el segundo es para atributos o bien si es una estructura de datos,
    //  el tercero es la función que se va a ejecutar y el cuarto es un argumento que se le puede pasar a la función
    // esta variable devulve un entero, un 1 si se creo correctamente y un 0 si no se creo, por lo tanto se valida
    int resultado = pthread_create(&hilo, NULL, saludoHilo, NULL);
    if (resultado != 0) {
        printf("Error al crear el hilo");
        return 1;
    } 

    //esto es para que el thread principal no termine antes que el creado por nosotros
    pthread_join(hilo, NULL);

    //debemos indicarle al compilador que estamos usando la libreria
    //gcc Proyecto1.c -o Proyecto1.out -lpthread

    
    

    int algoritmo;
    printf("Seleccione un algoritmo de planificación:\n");
    mostrarAlgoritmos(listaAlgoritmos, 4);
    scanf("%d", &algoritmo);
    printf("Algoritmo seleccionado: %s\n", listaAlgoritmos[algoritmo - 1].nombre);

    int opcion;
    printf("Seleccione una opcion:\n");
    printf("1. Manual \n");
    printf("2. Automatica \n");
    scanf("%d", &opcion);
    switch (opcion) {
        case 1:
            lecturaManual(algoritmo);
            break;
        case 2:
            lecturaAutomatica(algoritmo);
            break;
        default:
            printf("Opcion no valida\n");
    }

    liberarLista(listaProcesos);
    

    return 0;
}