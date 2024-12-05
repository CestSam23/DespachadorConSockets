#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include "mylib.h"
#define PUERTO 5050
#define IP "172.27.240.192"

int sock;
void client();
void configureSocket();
process_t newProcess();
struct sockaddr_in server_info;
struct hostent *host;
lista_t newLot(int n);

int main(){
    configureSocket();
	client();
	close(sock);

}

void configureSocket(){
    sock=socket(AF_INET,SOCK_STREAM,0);
    if (sock < 0) {
        perror("Error al crear el socket");
        exit(1);
    }    

    host=gethostbyname(IP);
    if (host == NULL) {
        perror("Error al obtener el host");
        exit(1);
    }

    memset(&server_info, 0, sizeof(server_info));
    memcpy(&server_info.sin_addr, host->h_addr, host->h_length);
    
	server_info.sin_family=AF_INET;
	server_info.sin_port=htons(PUERTO);
    if (connect(sock, (struct sockaddr *)&server_info, sizeof(server_info)) < 0) {
        perror("Error al conectar al servidor");
        exit(1);
    }
}

int menu(){
    int choice;
        printf("-------------Despachador--------------\n");
        printf("1. Nuevo proceso\n");
        printf("2. Salir\n");
        printf(".-");

        scanf("%d",&choice);    
        getchar();
        return choice;
}

void client(){
    int choice;

    while((choice=menu())!=2){

        switch (choice){
            case 1:
                //Nuevo proceso
                int n;
                printf("Number of processes to write: ");
                scanf("%d",&n);
                lista_t list = newLot(n);
                
                if (write(sock, &list, sizeof(lista_t)) < 0) {
                    perror("Error al enviar el proceso al servidor");
                }
                while(n>0){
                    process_t response;
                    read(sock,&response,sizeof(process_t));
                    printf("Proceso %d completado. Tiempo de espera: %d, Tiempo de terminación: %d\n", response.id, response.tWaiting, response.tCompletition);
                    n--;
                }
                break;
            default:
                //Intentar de nuevo
                printf("Intentar nuevamente\n");
                break;
        }
    }
}

lista_t newLot(int n){
    printf("--------------Nuevo Lote------------------\n");
    lista_t toReturn = {0};
    memset(&toReturn, 0, sizeof(lista_t));
    for(int i=0;i<n;i++){
        printf("----------Proceso %d-----------------\n",i+1);
        process_t newOne = {0};
        printf("Id: ");
        scanf("%d", &newOne.id);
        printf("Cpu Burst time: ");
        scanf("%d", &newOne.cpuBurst);
        getchar();  // Limpia el salto de línea residual en el buffer

        printf("Name: ");
        fgets(newOne.name, sizeof(newOne.name), stdin);

        newOne.name[strcspn(newOne.name, "\n")] = '\0'; // Elimina el salto de línea si existe
        addProcess(&toReturn,newOne);
    }
    return toReturn;
}

process_t newProcess(){
    printf("--------------Nuevo Proceso------------\n");
    process_t newOne = {0};
    printf("Id: ");
    scanf("%d", &newOne.id);
    printf("Cpu Burst time: ");
    scanf("%d", &newOne.cpuBurst);
    printf("Priority: ");
    scanf("%d", &newOne.priority);
    getchar();  // Limpia el salto de línea residual en el buffer

    printf("Name: ");
    fgets(newOne.name, sizeof(newOne.name), stdin);

    newOne.name[strcspn(newOne.name, "\n")] = '\0'; // Elimina el salto de línea si existe
    printf("Name: %s\n",newOne.name);

    return newOne;
}
