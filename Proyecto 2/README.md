# Proyecto 2 - Simulador de Asignacion de Memoria

Este proyecto simula la asignacion y liberacion de memoria para procesos usando **Paginacion** o **Segmentacion**, con sincronizacion por semaforos y comunicacion por memoria compartida.

La idea general es que los programas no se pasan texto por entrada o salida entre ellos, sino que cooperan usando recursos IPC del sistema operativo.

## Que programas hay y para que sirve cada uno

### 1) inicializador
Crea el ambiente IPC una sola vez:
- Memoria compartida con `shmget` y `shmat`.
- Semaforos System V con `semget` y `semctl`.
- Archivo de bitacora `bitacora.txt`.

Pide al usuario:
- Cantidad de frames.
- Esquema de trabajo: Paginacion o Segmentacion.

Cuando termina, sale y deja todo listo para los demas programas.

### 2) productor
Se conecta a la memoria y a los semaforos ya creados, y genera hilos que simulan procesos:
- Crea parametros aleatorios.
- Entra en region critica para buscar espacio.
- Si encuentra espacio, asigna, registra, duerme y libera.
- Si no encuentra espacio, registra que no hubo suficiente memoria.

### 3) espia
Permite ver en consola el estado del sistema:
- Estado de la memoria.
- Estado de los procesos.
- Bitacora de eventos.

Es util para demostrar en vivo como cambia el sistema mientras corre la simulacion.

### 4) finalizador
Hace limpieza total del sistema:
- Intenta terminar procesos activos.
- Limpia frames y tabla de procesos.
- Elimina memoria compartida y semaforos con `IPC_RMID`.
- Deja registro de cierre en la bitacora.

## Flujo de ejecucion

El flujo correcto es el siguiente:

1. Ejecutar `inicializador` una sola vez.
2. Ejecutar `productor`.
3. Ejecutar `espia` en paralelo si se quiere monitorear la simulacion.
4. Al final, ejecutar `finalizador` para liberar todos los recursos.

La secuencia general queda asi:

`inicializador -> (productor + espia) -> finalizador`

No es un servidor de red; es cooperacion local por IPC entre programas independientes.

## VI. Casos de prueba

### 1) Asignacion de memoria
**Que se queria probar:** que el productor pudiera reservar frames o segmentos correctamente segun el esquema elegido.

**Como se hizo:** se ejecuto primero el inicializador, luego el productor, y se observo si los procesos lograban entrar a memoria sin conflicto.

**Resultado:** los procesos que encontraron espacio fueron asignados correctamente y quedaron registrados en memoria compartida y en la bitacora.

### 2) Utilizacion de la memoria asignada
**Que se queria probar:** que un proceso, una vez asignado, permaneciera en memoria durante su tiempo de simulacion antes de liberar sus recursos.

**Como se hizo:** se permitio que el productor generara procesos con tiempos aleatorios y se verifico su permanencia antes de la liberacion.

**Resultado:** el proceso permanecio en estado activo durante su tiempo definido y despues libero la memoria asignada.

### 3) Visualizacion de la memoria
**Que se queria probar:** que el espia mostrara el estado actual de la memoria de forma clara.

**Como se hizo:** se ejecuto el espia mientras el productor estaba corriendo y se revisaron los frames ocupados y libres.

**Resultado:** el espia mostro correctamente la distribucion de memoria y permitio identificar los procesos presentes.

### 4) Limpieza de memoria
**Que se queria probar:** que al terminar la simulacion no quedaran recursos ocupados ni procesos registrados de forma incorrecta.

**Como se hizo:** se ejecuto el finalizador al terminar la corrida principal y se reviso que la memoria compartida y los semaforos fueran eliminados.

**Resultado:** el sistema libero los recursos IPC y dejo el entorno limpio para una nueva ejecucion.

### 5) Produccion de procesos
**Que se queria probar:** que el productor pudiera crear varios procesos simulados y sincronizarlos sin que dos ocuparan la misma region critica al mismo tiempo.

**Como se hizo:** se lanzo el productor con varios hilos y se verifico la bitacora de accesos y asignaciones.

**Resultado:** la sincronizacion evito choques en la asignacion y el registro de eventos quedo ordenado en la bitacora.

## VII. Que se queria probar

En este proyecto se queria comprobar principalmente que el programa inicializador creara correctamente la memoria compartida y los semaforos para que los demas programas pudieran trabajar sobre el mismo entorno.

Tambien se quiso probar que la simulacion respetara la region critica al asignar y liberar memoria, evitando que dos procesos ocuparan el mismo espacio al mismo tiempo.

Por ultimo, se busco verificar que el espia permitiera observar el estado del sistema y que el finalizador dejara el entorno completamente limpio al terminar.

## VIII. Resumen de experiencias

Durante la elaboracion del proyecto se practico el uso de memoria compartida, semaforos System V y control de estados de procesos. Uno de los aprendizajes mas importantes fue entender que la sincronizacion no solo sirve para evitar errores, sino tambien para mantener ordenada la informacion compartida entre programas separados.

Tambien fue util ver la diferencia entre programar un flujo secuencial y programar una simulacion concurrente. Al manejar varios procesos simulados, fue necesario pensar en estados, bloqueos, exclusiones mutuas y limpieza de recursos al final.

En general, el proyecto ayudo a reforzar la idea de que en sistemas operativos no basta con que un programa funcione por separado; tambien debe cooperar correctamente con otros procesos dentro del sistema.

## IX. Referencias bibliograficas

- Silberschatz, A., Galvin, P. B., y Gagne, G. - *Operating System Concepts*.
- Tanenbaum, A. S., y Bos, H. - *Modern Operating Systems*.
- Documentacion de Linux para `shmget`, `shmat`, `semget`, `semctl` y `semop`.
- Manuales del sistema en Linux: `man 2 shmget`, `man 2 shmat`, `man 2 semget`, `man 2 semctl`, `man 2 semop`.

## Como compilar y ejecutar el programa

Desde la carpeta del Proyecto 2, compila asi:

```bash
gcc -Wall -Wextra -o inicializador inicializador.c
gcc -Wall -Wextra -pthread -o productor productor.c
gcc -Wall -Wextra -o espia espia.c
gcc -Wall -Wextra -o finalizador finalizador.c
```

Luego ejecuta en este orden:

### Terminal 1
```bash
./inicializador
```

### Terminal 2
```bash
./productor
```

### Terminal 3
```bash
./espia
```

### Al terminar
```bash
./finalizador
```

## Anexo: bitacora de trabajo

La bitacora del sistema se guarda en el archivo `bitacora.txt`. Alli se registran eventos como:
- asignacion de memoria,
- liberacion de memoria,
- procesos que no encontraron espacio,
- y cierre del sistema.

Si se desea incluirla como anexo en el informe final, basta con adjuntar ese archivo como evidencia del comportamiento del proyecto.

## Nota final

Siempre termina la demo ejecutando `finalizador`, para no dejar recursos IPC huerfanos en el sistema.
