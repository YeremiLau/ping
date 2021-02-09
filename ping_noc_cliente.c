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
    
    struct sockaddr_in respondent, server;
    int tamano = sizeof(struct sockaddr_in);
    struct hostent *serverIP;

    char data[MAXDATA]; // contiene un string de tamano MAXDATA (el mensaje)
    int port = 0; // numero de puerto en el que se establece el socket
    int dataSize = 0; // tamano de  los datos recibidos

    signal(SIGINT, INThandler); // funcion capturar señal Ctrl + C (SIGINT signal interrupt)

    // Estructura que especifica el tiempo de espera de recvfrom 
    // Evitamos que se quede indefinidamente esperando recvfrom (socket bloqueante)
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

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
    respondent.sin_family = AF_INET; // 'sin.family' indica IPv4 (siempre será AF_INET)
    respondent.sin_port = htons(port); // almacena el puerto al que se asigna el socket
    respondent.sin_addr.s_addr = htonl(INADDR_ANY); // almacena la direccion IP (INADDR_ANY asigna calquier IP para el que cotesta)(para el server la que devuelve gethostbyname)

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr = *((struct in_addr *)serverIP->h_addr);

    // CREACION del socket NOC: int socket(int dominio, int tipo, int protocolo);
    // 'dominio' AF_INET (IPv4)
    // 'tipo' SOCK_DGRAM soporta datagramas (NOC)
    // 'protocolo' 0 (corresponde a IP)
    // Si el descriptor que devuelve es menos a 0 se ha producido un error
    if ((socketfd = socket(AF_INET, SOCK_DGRAM , 0)) < 0)
    {
        perror("ERROR: Error en la creacion del socket.");
        exit(-1);
    }

    // Establecemos que el socket pase bloqueado hasta 5 seg en recvfrom:
    // int setsockopt(int sockfd, int level, int option_name, const void *option_value, int long);
    // 'sockfd' descriptor del socket
    // 'level' nivel de protocolo. Establece opciones a nivel de socket 'SOL_SOCKET'
    // 'option_name' (SO_RCVTIMEO) establece el tiempo max que debe esperar la funcion de entrada (recvfrom)
    // 'option_value' valor de la opcion (estructura timeval timeout)
    // 'long' longitud de la estructura timeval
    setsockopt(socketfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(struct timeval));

    printf("Socket creado\n PING (%s) %d bytes of data.\n", inet_ntoa(server.sin_addr), MAXDATA);

    // Espera de peticiones (buble infinito)
    while (1)
    {
        // Envio de petición eco.
        // ssize_t sendto(int sockfd, const void *message, size_t longMsg, int type, const struct sockaddr *dest_addr, tamano)
        // 'sockfd': descriptor del socket
        // 'message': mensaje a enviar
        // 'longMsg': longitud del mensaje
        // 'type': tipo de mensaje a transmitir 
        // 'des_addr': estructura con la direccion IP/puerto de destino
        // 'tamano': tamano de la estructura anterior
        // Si el valor devuelto por sendto() es menor a 0, salimos y cerramos el socket
        // sprinft genera el mensaje a enviar. Lo almacena en data.
        sprintf(data, "ping request %d", ++transmitted);

        if (sendto(socketfd, (const char *)data, MAXDATA, 0, (struct sockaddr *)& server, tamano) < 0)
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
        // Espera de respuesta eco.
        // ssize_t recvfrom(int sockfd, void *message, size_t longMsg, int type, struct sockaddr *src_addr, tamano);
        // 'sockfd': descriptor del socket
        // 'Message': puntero donde se almacenara el mensae recibido
        // 'longMsg': longitud del mensaje recibido
        // 'type': tipo de mensaje a transmitir
        // 'src_addr': contiene la direccion IP/puerto de origen del mensaje
        // 'tamano': tamano de la estructura anterior
        // Si el valor devuelto por recvfrom es menor a 0, comprobamos si el numero de error es distinto a EAGAIN o EWOULDBLOCK para salir o no de programa.
        // Si no es menor a 0, mostramos el mensaje que se ha recibido.
        if ((dataSize = recvfrom(socketfd, (char *)data, MAXDATA, 0, (struct sockaddr *)& respondent, &tamano)) < 0)
        {
            if (!(errno == EAGAIN || errno == EWOULDBLOCK))
            {
                perror("ERROR: Error en la recepcion de la respuesta.");
                close(socketfd);
                exit(-1);
            }
            printf("Tiempo de espera agotado para la peticion %d.\n", transmitted);
        }
        // Si no hay error, mostramos el mensaje, incrementamos received y esperamos la siguiente peticion
        else
        {
            // Segunda marca de tiempo (la diferencia será el tiempo que tarda en enviar/recibir)
            fin = clock();
            printf("%d bytes from %s: %s time=%0.3f ms\n", dataSize, inet_ntoa(respondent.sin_addr), data, ((double) 1000*(fin - ini)) / CLOCKS_PER_SEC);
            received++;
            sleep(1);
        }
    }
    // En caso de que falle el buble (puede fallas sleep) se cierra el socket y se sale del programa 
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
