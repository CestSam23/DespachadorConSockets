#include "mylib.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <strings.h>
#include <netdb.h>
#include <sys/un.h>
#define MAX_PROCESSES 1000
#define QUANTUM 5

/*
COBIX LOPEZ HANIA CELESTE
FLORES HERNANDEZ ALESSANDRA
RODRIGUEZ BRAVO HUGO JAVIER
TAPIA GALVEZ SAMUEL FERNANDO
*/

/* FIFO */
int fd;
char *ruta="/tmp/stats";

/* SEMAFOROS Y MEMORIA COMPARTIDA */
int shm_id, sem_id, empty_count; // Identificadores de Semáforos
lista_t *shared_data; // Identificadores de Memoria Compartida
pid_t despachador;

void down(int sem_id);
void up(int sem_id);

/* MODULO LARGO PLAZO */
void LargoPlazo();

/* MODULO CORTO PLAZO */
void CortoPlazo();

/*  MODULO ESTADISTICA*/
int countProcessExecuted = 0, countProcessAnnihilated = 0, totalWaitTime = 0; // variables para contadores de procesos
FILE *ProcessDispatcherStatistics; // archivo generado para estadisticas
void estadisticas();

/* FUNCIONES DE FINALIZACIÓN */ 
void manejar_sigterm(int sig);
void freeResources(int semid, int shmid, lista_t *shm_ptr);

//SOCKETS
int client_socket; //Descriptor de socket que regresa funcion accept
int sock; //Socket
#define PUERTO 5050
void configureSocket();

int main(){
    // Inicialización de la memoria compartida y semáforos
    shm_id = shmget(IPC_PRIVATE, sizeof(lista_t), IPC_CREAT | 0666);
    shared_data = (lista_t *)shmat(shm_id, NULL, 0);

    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);   // Semáforo para acceso exclusivo a la memoria compartida
    empty_count = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666); // Semáforo para contar si hay procesos disponibles

    semctl(sem_id, 0, SETVAL, 1);      // Inicializamos semáforo para acceso exclusivo a memoria compartida
    semctl(empty_count, 0, SETVAL, 0); // Inicializamos el contador de procesos en memoria como vacío

    //SOCKETS
    configureSocket();

    //fifo
    mkfifo(ruta,0666);

    despachador=fork();
    if (despachador == 0) {
        LargoPlazo();
    } else {
        signal(SIGTERM, manejar_sigterm);
        signal(SIGINT, manejar_sigterm);
        CortoPlazo();
    }

}

/* MANEJO DE SEMÁFOROS */
void down(int sem_id) {
    struct sembuf op = {0, -1, 0};
    semop(sem_id, &op, 1);
}

void up(int sem_id) {
    struct sembuf op = {0, 1, 0};
    semop(sem_id, &op, 1);
}

/* LARGO PLAZO */

void LargoPlazo(){
    printf("Servidor en espera de conexiones...\n");

    while (1) {
        int client_socket = accept(sock, (struct sockaddr *)0, (int *)0);
        if (client_socket < 0) {
            perror("Error al aceptar conexión");
            continue;
        }

        if (fork() == 0) {  // Proceso hijo
            close(sock); // El hijo no necesita el socket principal
            printf("Conexión creada\n");

            
            

            while (1) {
                process_t response = {0};
                lista_t readedList={0};
                memset(&readedList, 0, sizeof(lista_t)); // Limpieza antes de leer
                ssize_t bytes_read = read(client_socket, &readedList, sizeof(lista_t));
                
                printf("Lote Recibido\n");
                if (bytes_read <= 0) {
                    if (bytes_read == 0) {
                        printf("Cliente desconectado\n");
                    } else {
                        perror("Error al leer datos del cliente");
                    }
                    break;
                }

                if(isEmpty(&readedList)) break;

                toString(&readedList);

                printf("Creando Estructura\n");
                int n;
                process_t add[(n=size(&readedList))];

                printf("LLevando lista a inicio\n");
                rewindList(&readedList);
                for(int i=0;i<n;i++){
                    add[i] = deleteProcess(&readedList);
                    printf("Proceso añadido al arreglo %s\n",add[i].name);
                }
                // Semáforos para acceso a memoria compartida
                down(sem_id);  // Bloquea acceso
                printf("Añadiendo a memoria dinámica\n");
                addProcesses(shared_data, add,n);
                up(empty_count); // Indica que hay un nuevo proceso
                up(sem_id);      // Libera acceso


                //Respuesta recibida. SERVER
                //LEER RESPUESTA
                fd=open(ruta, O_RDONLY);
                while(n>0){
                    read(fd, &response ,sizeof(process_t));
                    write(client_socket,&response,sizeof(process_t));
                    n--;
                }
                close(fd);

            }

            close(client_socket);
            exit(0);
        } else {
            close(client_socket); // El padre no necesita este socket
        }
    }
}

/* CORTO PLAZO */

lista_t listaDeProcesos = {-1,0};
void roundRobin();
void priority();
void sendToAnalytics(process_t process);

void CortoPlazo(){
    while (1) {
        down(empty_count);  // Esperar hasta que haya procesos disponibles
        down(sem_id);  // Sincronizar acceso a la memoria compartida
        
        // Leer los datos de la memoria compartida y almacenarlos en listaDeProcesos
        printf(" ----> Procesos disponibles para leer: %d\n\n", size(shared_data));  // Depuración: cuántos procesos hay
        for (int i = 0; i < size(shared_data); i++) {
            printf(" . Proceso leido: %s\n", actual(shared_data).name);  // Depuración: muestra los procesos leídos
            addProcess(&listaDeProcesos,actual(shared_data));
            next(shared_data);
        }

        while(isEmpty(shared_data)!=1){
            deleteProcess(shared_data);
        }

        up(sem_id);  // Liberar acceso a la memoria compartida
        
        // Despachar procesos
        if (size(&listaDeProcesos) > 0) {
            roundRobin();
            priority();
        }
        printf("\n - - L O T E   D E S P A C H A D O - - \n");
    }
}


void roundRobin(){
    // Iniciamos el Round Robin en la lista de procesos
    rewindList(&listaDeProcesos);
    int iterations = size(&listaDeProcesos);
    for (int i = 0; i < iterations; i++) {
        printf(" --> Proceso: %s\n", actual(&listaDeProcesos).name);
        // Aquí es donde manejas el tiempo de CPU
        
        if(actual(&listaDeProcesos).cpuBurst>=5){
            aumentarEspera(&listaDeProcesos,QUANTUM);
        } else {
            aumentarEspera(&listaDeProcesos, actual(&listaDeProcesos).cpuBurst % QUANTUM);
        }
        if (restarEjecucion(&listaDeProcesos, QUANTUM) == -1) {
            printf(" -- El proceso %s ha sido despachado por completo con RoundRobin --\n", actual(&listaDeProcesos).name);
            toString(&listaDeProcesos);
            sendToAnalytics(deleteProcess(&listaDeProcesos));
            //printf("*******************************************************\n\n");

        } else {
            aumentarTerminacion(&listaDeProcesos, QUANTUM);
            printf("\t.. Despachando proceso: %s\n", actual(&listaDeProcesos).name);
            printf("Avanzo a siguiente\n");
            next(&listaDeProcesos);

            //printf("*******************************************************\n\n");
        }
    }
}

void priority(){
    // Despachar por prioridades
    int iterations = size(&listaDeProcesos);
    for (int i = 0; i < iterations; i++) {
        printf(" --> Proceso: %s \n", actual(&listaDeProcesos).name);
        printf(" . Despachando proceso:%s\n", actual(&listaDeProcesos).name);

        int remain = actual(&listaDeProcesos).cpuBurst;
        aumentarEspera(&listaDeProcesos, remain);
        aumentarTerminacion(&listaDeProcesos, remain);
        restarEjecucion(&listaDeProcesos, remain);

        printf("El proceso %s ha sido despachado por completo con prioridades\n", actual(&listaDeProcesos).name);
        sendToAnalytics(deleteProcess(&listaDeProcesos));
    }
}

void sendToAnalytics(process_t process){
    if (process.tCompletition > 0) { 
        countProcessExecuted++; 
    } else { 
        countProcessAnnihilated++; 
    }
    totalWaitTime += process.tWaiting;

    //ENVIAR RESPUESTA
    //WRITE FIFO PROCESS
    fd=open(ruta, O_WRONLY);
    write(fd, &process,sizeof(process_t));
    close(fd);
}


/* ESTADISTICA */
void estadisticas() {
    double averageWaitingTime = countProcessExecuted > 0 ? 
        (double)totalWaitTime / countProcessExecuted : 0.0;

    printf("Estadísticas del despachador:\n");
    printf("Procesos ejecutados: %d\n", countProcessExecuted);
    printf("Procesos aniquilados: %d\n", countProcessAnnihilated);
    printf("Tiempo promedio de espera: %.2f\n", averageWaitingTime);

    ProcessDispatcherStatistics = fopen("estadisticas.txt", "w");
    if (ProcessDispatcherStatistics != NULL) {
        fprintf(ProcessDispatcherStatistics, "Estadísticas del despachador:\n");
        fprintf(ProcessDispatcherStatistics, "Procesos ejecutados: %d\n", countProcessExecuted);
        fprintf(ProcessDispatcherStatistics, "Procesos aniquilados: %d\n", countProcessAnnihilated);
        fprintf(ProcessDispatcherStatistics, "Tiempo promedio de espera: %.2f\n", averageWaitingTime);
        fclose(ProcessDispatcherStatistics);
    }
}

/* FINALIZACION */
// Función para manejar SIGTERM
void manejar_sigterm(int sig) {
    printf("\nRecibiendo señal SIGTERM, generando estadísticas...\n");
    estadisticas();
    freeResources(sem_id, shm_id, shared_data);
    exit(0);
}

// Función de liberación de recursos
void freeResources(int semid, int shmid, lista_t *shm_ptr) {
    shmdt(shm_ptr);              // Desvincula la memoria compartida
    shmctl(shmid, IPC_RMID, 0);  // Elimina la memoria compartida
    semctl(semid,IPC_RMID,0);
}

/*SOCKETS*/
void configureSocket(){
    struct sockaddr_in local;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error al crear socket");
        exit(1);
    }

    local.sin_family = AF_INET;
    local.sin_port = htons(PUERTO);
    local.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("Error al enlazar socket");
        close(sock);
        exit(1);
    }

    if (listen(sock, 5) < 0) {
        perror("Error al escuchar en socket");
        close(sock);
        exit(1);
    }
}
