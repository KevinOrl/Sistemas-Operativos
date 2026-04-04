#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include<unistd.h>

void *saludoHilo() {
    printf("Hola desde el hilo!\n");
}

int main() {
    pthread_t hilo;

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


    FILE *archivo = fopen("Procesos.txt", "r");
    if (archivo == NULL) {
        perror("No se pudo abrir Procesos.txt");
        return 1;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo) != NULL) {
        int burst = 0;
        int prioridad = 0;

        if (sscanf(linea, "%d %d", &burst, &prioridad) == 2) {
            printf("Burst: %d, Prioridad: %d\n", burst, prioridad);
        } else {
            printf("Linea invalida: %s", linea);
        };
        sleep(rand() % 6 + 3); 
    }

    fclose(archivo);

    return 0;
}