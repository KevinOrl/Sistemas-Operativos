/*
inicializador.c

Crea y configura todos los recursos IPC del sistema:
- Memoria compartida (shmget/shmat)
- Semáforos System V (semget/semctl)
- Archivo de bitácora

Uso: ./inicializador

Debe ejecutarse solo 1 vez antes que cualquier otro programa.
Termina al finalizar la inicialización.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"

// Prototipos
static void pedir_parametros(int *total_frames, Esquema *esquema);
static int  crear_shm(int total_frames, Esquema esquema);
static int  crear_semaforos(void);
static void inicializar_log(int total_frames, Esquema esquema);

// IMPLEMENTACIONES
static void pedir_parametros(int *total_frames, Esquema *esquema) {
    int opcion;

    // Cantidad de frames
    //Ciclo que hace que el usuario ingrese un número entero entre 1 y MAX_FRAMES,
    //validando la entrada, hasta que entre un numero valido
    do {
        printf("Ingrese la cantidad de frames de memoria (1-%d): ", MAX_FRAMES);
        //escaneo de la cantidad de frames, validando que sea un entero
        if (scanf("%d", total_frames) != 1) {
            // ciclo para limpiar el buffer de entrada en caso de que 
            // el usuario ingrese algo que no sea un número entero
            while (getchar() != '\n');  // limpiar buffer
            *total_frames = 0;
        }
        // Valida rango y tira mensaje de error si no es válido
        if (*total_frames < 1 || *total_frames > MAX_FRAMES)
            printf("  Error: Valor fuera de rango. Intente de nuevo.\n");
    } while (*total_frames < 1 || *total_frames > MAX_FRAMES);

    // Esquema de asignación
    do {
        printf("Seleccione el esquema:\n");
        printf("  0) Paginacion\n");
        printf("  1) Segmentacion\n");
        printf("Opcion: ");
        if (scanf("%d", &opcion) != 1) {
            while (getchar() != '\n');
            opcion = -1;
        }
        if (opcion != 0 && opcion != 1)
            printf("  Error: Opcion invalida. Intente de nuevo.\n");
    } while (opcion != 0 && opcion != 1);

    // Asignar esquema basado en opción
    // quedaria por ejemplo *esquema = PAGINACION si opcion es 0, o SEGMENTACION si opcion es 1
    *esquema = (Esquema)opcion;
}

static int crear_shm(int total_frames, Esquema esquema) {
    // IPC_EXCL garantiza que no exista ya un segmento con esta clave
    // reserva memoria de acuerdo a lo que puede guardar el struct MemoriaCompartida
    int shmid = shmget(SHM_KEY,
                       sizeof(MemoriaCompartida),
                       IPC_CREAT | IPC_EXCL | 0666);
    // Si falla, probablemente ya exista un segmento con esa clave (no se limpió bien)
    if (shmid < 0) {
        perror("  ✗ shmget");
        fprintf(stderr,
                "  → ¿Ya hay memoria compartida creada?\n"
                "    Ejecute primero ./finalizador y vuelva a intentar.\n");
        exit(EXIT_FAILURE);
    }

    // Mapear para inicializar
    MemoriaCompartida *mem = (MemoriaCompartida *)shmat(shmid, NULL, 0);
    if (mem == (void *)-1) {
        perror("  ✗ shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    // Inicializar toda la estructura a cero primero
    memset(mem, 0, sizeof(MemoriaCompartida));

    // Configurar campos globales
    mem->esquema      = esquema;
    mem->total_frames = total_frames;
    mem->num_procesos = 0;

    // Frames: todos libres
    for (int i = 0; i < total_frames; i++) {
        mem->frames[i].estado   = FRAME_LIBRE;
        mem->frames[i].pid      = -1;
        mem->frames[i].segmento = -1;
    }

    // Tabla de procesos: todas las entradas vacías
    for (int i = 0; i < MAX_PROCESSES; i++) {
        mem->procesos[i].estado = PROC_VACIO;
        mem->procesos[i].pid   = -1;
    }

    shmdt(mem);

    printf("  Memoria compartida creada  (shmid=%d, %zu bytes)\n",
           shmid, sizeof(MemoriaCompartida));
    return shmid;
}

static int crear_semaforos(void) {
    int semid = semget(SEM_KEY, NUM_SEMS, IPC_CREAT | IPC_EXCL | 0666);
    if (semid < 0) {
        perror("  Error semget");
        fprintf(stderr,
                "  → ¿Ya existen semáforos con esa clave?\n"
                "    Ejecute primero ./finalizador y vuelva a intentar.\n");
        // Limpiar shm antes de salir
        int shmid2 = shmget(SHM_KEY, 0, 0666);
        if (shmid2 >= 0) shmctl(shmid2, IPC_RMID, NULL);
        exit(EXIT_FAILURE);
    }

    /*
    Inicializar los 3 semáforos en 1 (mutex binario):
    SEM_MEMORIA (0) → protege búsqueda/liberación en memoria
    SEM_LOG     (1) → protege escritura en bitácora
    SEM_TABLA   (2) → protege tabla de procesos
    */
    union semun arg;
    unsigned short vals[NUM_SEMS] = {1, 1, 1};
    arg.array = vals;

    if (semctl(semid, 0, SETALL, arg) < 0) {
        perror("  Error semctl SETALL");
        semctl(semid, 0, IPC_RMID, arg);
        exit(EXIT_FAILURE);
    }

    printf("  Semaforos creados          (semid=%d, cantidad=%d)\n",
           semid, NUM_SEMS);
    return semid;
}

static void inicializar_log(int total_frames, Esquema esquema) {
    FILE *f = fopen(LOG_FILE, "w");   // 'w' → sobreescribir si existía
    if (!f) {
        perror("  ✗ fopen bitácora");
        exit(EXIT_FAILURE);
    }

    time_t now = time(NULL);
    fprintf(f, "|------------------------------------------------------|\n");
    fprintf(f, "|          Bitácora del Sistema de Memoria             |\n");
    fprintf(f, "|------------------------------------------------------|\n");
    fprintf(f, "|  Inicio   : %s", ctime(&now));
    fprintf(f, "|  Esquema  : %-40s|\n",
            esquema == PAGINACION ? "Paginacion" : "Segmentacion");
    fprintf(f, "|  Frames   : %-40d|\n", total_frames);
    fprintf(f, "|------------------------------------------------------|\n\n");

    fclose(f);
    printf("  ✓ Bitácora inicializada      (%s)\n", LOG_FILE);
}

// MAIN
int main(void) {
    int    total_frames;
    Esquema esquema;

    printf("\n|------------------------------------------|\n");
    printf(  "|   Inicializador del Sistema de Memoria   |\n");
    printf(  "|------------------------------------------|\n\n");

    // 1. Solicitar parámetros al usuario
    pedir_parametros(&total_frames, &esquema);

    // 2. Crear e inicializar memoria compartida
    int shmid = crear_shm(total_frames, esquema);

    // 3. Crear semáforos
    int semid = crear_semaforos();

    // 4. Inicializar bitácora
    inicializar_log(total_frames, esquema);

    // 5. Resumen
    printf("\n|------------------------------------------|\n");
    printf("|            Sistema inicializado          |\n");
    printf("|------------------------------------------|\n");
    printf("|  shmid   : %-28d │\n", shmid);
    printf("|  semid   : %-28d │\n", semid);
    printf("|  Frames  : %-28d │\n", total_frames);
    printf("|  Esquema : %-28s │\n",
           esquema == PAGINACION ? "Paginacion" : "Segmentacion");
    printf("|  Log     : %-28s │\n", LOG_FILE);
    printf("|-----------------------------------------|\n\n");
    printf("Inicializador terminando. Ya puede lanzar los demás programas.\n\n");

    return 0;
}