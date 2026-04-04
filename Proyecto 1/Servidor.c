//Librerias para el server
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

int main(int argc, char **argv){
    if(argc>1){

        //Definir variables
        int fd,fd2,longitudCliente,puerto;
        puerto=atoi(argv[1]);

        //Struct para el cliente y servidor
        struct sockaddr_in servidor,cliente;

        //Configuracion del servidor
        servidor.sin_family=AF_INET; //Familia de protocolos TCP/IP
        servidor.sin_port=htons(puerto);
        servidor.sin_addr.s_addr=INADDR_ANY;
        bzero(&servidor.sin_zero,8);
        
        //Creacion del socket
        if((fd=socket(AF_INET,SOCK_STREAM,0))<0){
            perror("Error al crear el socket");
            exit(1);
        }

        //Avisar al sistema de la creacion del socket y unirlo al puerto
        if(bind(fd,(struct sockaddr *)&servidor, sizeof(struct sockaddr))==-1){
            printf("Error en bind()");
            exit(1);
        }

        //Poner el socket en modo escucha
        if(listen(fd,5)==-1){
            printf("Error en listen()");
            exit(1);
        }

        //Aceptar conexiones entrantes
        while(1){
            longitudCliente=sizeof(struct sockaddr_in);

            if((fd2=accept(fd,(struct sockaddr *)&cliente,&longitudCliente))==-1){
                printf("Error en accept()");
                exit(1);
            }

            send(fd2, "Bienvenido al servidor", 26, 0);
            close(fd2);
        }

        close(fd);

    }
    else{
        printf("Error: No se ha ingresado el puerto por parametro");
        return 0;
    }
}