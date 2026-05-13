# Proyecto 2 - Simulador de Asignacion de Memoria (SO)

Este proyecto simula la asignacion y liberacion de memoria para procesos usando **Paginacion** o **Segmentacion**, con sincronizacion por semaforos y comunicacion por memoria compartida.

La idea general es que los programas NO se pasan texto por entrada/salida entre ellos, sino que cooperan usando recursos IPC del sistema operativo.

---

## Que programas hay y para que sirve cada uno

### 1) inicializador
Crea el ambiente IPC una sola vez:
- Memoria compartida (`shmget` + `shmat`)
- Semaforos System V (`semget` + `semctl`)
- Archivo de bitacora (`bitacora.txt`)

Pide al usuario:
- Cantidad de frames
- Esquema de trabajo (Paginacion o Segmentacion)

Cuando termina, sale. Deja todo listo para los demas.

### 2) productor
Se conecta a la memoria/semaforos ya creados y genera hilos (procesos simulados):
- Crea parametros aleatorios
- Entra en region critica para buscar espacio
- Si encuentra: asigna, registra, duerme, libera
- Si no encuentra: registra que no hubo espacio

### 3) espia
Permite ver en consola:
- Estado de la memoria
- Estado de procesos
- Bitacora

Es util para demostrar en vivo como cambia el sistema.

### 4) finalizador
Hace limpieza total del sistema:
- Intenta terminar procesos activos
- Limpia frames y tabla
- Elimina memoria compartida y semaforos (`IPC_RMID`)
- Deja registro de cierre en bitacora

---

## Flujo de ejecucion recomendado

El flujo correcto es este:

1. Ejecutar `inicializador` (una sola vez).
2. Ejecutar `productor`.
3. Ejecutar `espia` en paralelo (si quieres monitorear mientras corre el productor).
4. Al final, ejecutar `finalizador` para liberar todo.

Visualmente:

`inicializador -> (productor + espia) -> finalizador`

No es un servidor de red; es cooperacion por IPC local.

---

## Compilacion (Ubuntu 24)

Desde la carpeta del Proyecto 2:

```bash
gcc -Wall -Wextra -o inicializador inicializador.c
gcc -Wall -Wextra -pthread -o productor productor.c
gcc -Wall -Wextra -o espia espia.c
gcc -Wall -Wextra -o finalizador finalizador.c
```

---

## Ejecucion paso a paso

### Terminal 1
```bash
./inicializador
```

### Terminal 2
```bash
./productor
```

### Terminal 3 (opcional, monitoreo)
```bash
./espia
```

### Cuando termine la simulacion
```bash
./finalizador
```

---

## Que revisar si algo falla

- Si `inicializador` dice que ya existen recursos IPC, corre `./finalizador` y vuelve a iniciar.
- Si `productor` falla en `shmget` o `semget`, casi siempre es porque no corriste antes `inicializador`.
- Si quieres ver la traza completa, abre `bitacora.txt`.

---

## Mensaje corto para defensa del proyecto

"El sistema esta implementado como programas separados que cooperan por IPC (memoria compartida + semaforos System V). No usamos pipeline de stdin/stdout ni sockets, porque el enunciado pide sincronizacion entre procesos con memoria compartida y semaforos." 

---

## Nota practica

Siempre termina la demo ejecutando `finalizador`, para no dejar recursos IPC huerfanos en el sistema.
