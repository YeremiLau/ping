#include <stdio.h>
#include <stdlib.h>     // exit()
#include <unistd.h>     // close() sleep() 
#include <sys/types.h>  // recvfrom() sendto() 
#include <sys/socket.h> // socket() 
#include <netinet/in.h> // struct sockaddr_in 
#include <netdb.h>      // struct hostent 
#include <signal.h>     // signal() 
#include <arpa/inet.h>  // inet_ntoa() 
#include <errno.h>      // errno 
#include <time.h>       // clock() 

#define MAXDATA 64 // Tamano maximo del mensaje a enviar

// Variables GLOBALES:
// 'socket_fd' descriptor del socket
// 'transmitted' numero de peticiones enviadas
// 'received' numero de respuestas recibidas
int socketfd, transmitted = 0, received = 0;

// Senal de interrupcion Ctrl + C
void INThandler(int);

int main(int argc, char *argv[])
{
    // Variables LOCALES:
    // 'sockaddr_in' estructura para dominio internet. Se usa en bind(), sendto() y recvfrom().
    // 'longitud' tamano de un sockaddr_in. Se usa en sendto() y recvfrom().
    // 'serverIP' estructura que conteniene la IP del servidor devuelta gethostbyname
    
    struct sockaddr_in server;
    int tamano = sizeof(struct sockaddr_in);
    struct hostent *serverIP;

    char data[MAXDATA]; // contiene un string de tamano MAXDATA (el mensaje)
    int port = 0; // numero de puerto en el que se establece el socket
    int dataSize = 0; // tamano de  los datos recibidos

    signal(SIGINT, INThandler); // funcion capturar señal Ctrl + C (SIGINT signal interrupt)

    // 'ini' valor del clock cuando se envía una peticion
    // 'fin' valor cuando recibe la respuesta 
    clock_t ini, fin;

    // CONTROL DE ARGUMENTOS
    // Debe haber dos argumentos (IP del server + puerto)
    if (argc != 3)
    {
        printf("ERROR: numero de argumentos incorrecto.\nUso: ./ping_noc_cliente [IP] [PUERTO]\n");
        exit(-1);
    }
    // Asignamos y comprobamos la IP dada por el usuario
    if ((serverIP = gethostbyname(argv[1])) == NULL)
    {
        perror("ERROR: IP no valida.");
        exit(-1);
    }
   // Asignamos el puerto
    if ((port = atoi(argv[2])) < 1024) // ¿Es un peurto reservado? (<1024)
    {
        perror("ERROR: puerto reservado (<1024)");
        exit(-1);
    }
     // Inicializacion estructura sockaddr_in: 
    server.sin_family = AF_INET; // 'sin.family' indica IPv4 (siempre será AF_INET)
    server.sin_port = htons(port); // almacena el puerto al que se asigna el socket
    server.sin_addr = *((struct in_addr *)serverIP->h_addr); // almacena la direccion IP (la que devuelve gethostbyname)

    // CREACION del socket NOC: int socket(int dominio, int tipo, int protocolo);
    // 'dominio' AF_INET (IPv4)
    // 'tipo' SOCK_STREAM soporta protocolo conectivo (OC)
    // 'protocolo' 0 (corresponde a IP)
    // Si el descriptor que devuelve es menos a 0 se ha producido un error
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERROR: Error en la creacion del socket.");
        exit(-1);
    }
    printf("Socket creado\n");

    // Conexión del socket a la dirección del servidor.
    // int connect(int sockfd, const struct sockaddr *addr, int long);
    // 'sockfd' descriptor del socket
    // 'sockaddr' dirección del servidor
    // 'long' tamaño de un struct que contiene la IP del servidor
    // Si falla, se sale del programa
    if (connect(socketfd, (struct sockaddr*)& server, tamano) < 0) {
        perror("ERROR: Error al conectarse al socket.");
        exit(-1);
    }

    printf("Socket conectado\n PING (%s) %d bytes of data.\n", inet_ntoa(server.sin_addr), MAXDATA);

    // Espera de peticiones (buble infinito)
    while (1)
    {
        // Envio de petición echo.
        // int send(int sockfd, void *Msg, int longMsg, int opcion);
        // 'sockfd': descriptor del socket
        // 'Msg': mensaje a enviar
        // 'longMsg': longitud del mensaje a enviar
        // 'opcion': flags para el envio (0 o MSG_OOB o MSG_PEEK)
        // Si el valor devuelto por send es menor a 0, salimos y cerramos el socket
        // sprinft genera el mensaje a enviar. Se guarda en data
        sprintf(data, "ping request %d", ++transmitted);
        if (send(socketfd, (const char *)data, MAXDATA, 0) < 0)
        {
            perror("ERROR: error en la peticion.");
            close(socketfd);
            exit(-1);
        }
        else
        { // Almacenamos la primera marca del clock por donde va la ejecucion del prog (asi sabremos cuanto tarda en recibir respuesta)
            ini = clock();
            //printf("Petición %d enviada a %s\n", transmitted, inet_ntoa(server.sin_addr));
        }

        // Espera de respuesta echo.
        // int recv(int sockfd, void *Msg, int longMsg, int opcion);
        // 'sockfd': descriptor del socket
        // 'Msg': puntero donde se almacenara el mensaje recibido
        // 'longMsg': longitud del mensaje recibido
        // 'opcion': flags (0 o MSG_OOB o MSG_PEEK)
        //  Si el valor devuelto por recv es menor a 0, salimos y cerramos el socket
        if ((dataSize = recv(socketfd, (char *)data, MAXDATA, 0)) < 0)
        {
            // Si el servidor ha cerrado la conexion, avisamos y mostramos estadisticas
            if (errno == ECONNRESET)
            {
                printf("El servidor a finalizado la conexion.\n");
                printf("\n--- ping statistics ---\n%d packets transmitted, %d received, %0.2f%% packets loss\n", transmitted, received, 100*(1-((float)received/(float)transmitted)));
                close(socketfd);
                exit(-1);
            }
            else
            {
                perror("ERROR: Error en la recepcion de la respuesta.");
                close(socketfd);
                exit(-1);
            }
        }
        // Si no hay error, segunda marca de tiempo y mostramos estadisticas 
        else
        {
            fin = clock();
            printf("%d bytes from %s: %s time=%0.3f ms\n", dataSize, inet_ntoa(server.sin_addr), data, ((double) 1000*(fin - ini)) / CLOCKS_PER_SEC);
            received++;
            sleep(1);
        }
    }

    // En caso de que falle el buble (puede fallar sleep) se cierra el socket y se sale del programa 
    printf("ERROR: Se cerrara el programa.\n");
    close(socketfd);
    exit(-1);
}

// Funcion para capturar la señal Ctrl + C
void INThandler(int sig)
{
    char salir;

    printf("\nQuieres salir del programa? [y/n] ");
    fflush (stdout);
    salir = getchar();
    if (salir == 'y' || salir == 'Y')
    {
        printf("\n--- ping statistics ---\n%d packets transmitted, %d received, %0.2f%% packets loss\n", transmitted, received, 100*(1-((float)received/(float)transmitted)));
        close(socketfd);
        exit(0);
    }
}
