//Librerias para el cliente
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

int main(int argc, char **argv){
    if(argc>2){

        //Definir variables
        char *ip;
        int fd,numbytes,puerto;
        char buf[100];
        puerto=atoi(argv[2]);
        ip=argv[1];

        //Struct para el cliente y servidor
        struct sockaddr_in servidor;

        //Informacion sobre el nodo remoto
        struct hostent *he;

        //Obtener informacion del host
        if((he=gethostbyname(ip))==NULL){
            printf("Error al obtener informacion del host");
            exit(-1);
        }

        //Configuracion del servidor
        if ((fd=socket(AF_INET,SOCK_STREAM,0))==-1){
            perror("Error al crear el socket");
            exit(-1);
        }

        //datos del server
        servidor.sin_family=AF_INET; //Familia de protocolos TCP/IP
        servidor.sin_port=htons(puerto);
        servidor.sin_addr=*((struct in_addr *)he->h_addr);
        bzero(&(servidor.sin_zero),8);

        //Conectar al servidor
        if(connect(fd,(struct sockaddr *)&servidor,sizeof(struct sockaddr))==-1){
            printf("Error en connect()");
            exit(-1);
        }

        if ((numbytes=recv(fd,buf,100,0))==-1){
            printf("Error en recv()");
            exit(-1);
        }

        buf[numbytes]='\0';
        printf("Mensaje del servidor: %s\n",buf);

        close(fd);

    }
    else{
        printf("Error: No se ha ingresado el host y el puerto por parametro");
    }
}