#ifndef PRODUCTOR_H
#define PRODUCTOR_H

#include "common.h"

// Prototipo de la función principal del productor
typedef struct {
    pid_t pid;
    int tiempo;
    int num_paginas; // Para paginación
    int num_segmentos; // Para segmentación
    int tam_segmentos[MAX_SEGS]; // [2, 3]
} ParametrosProceso;

void crear_procesos();

#endif // PRODUCTOR_H
