#include <stdio.h>
#include <stdlib.h>     // exit()
#include <unistd.h>     // close() sleep() 
#include <netdb.h>      // struct hostent 
#include <signal.h>     // signal() 
#include <arpa/inet.h>  // inet_pton() 
#include <sys/types.h>  // recvfrom() sendto()
#include <sys/socket.h> // socket() 
#include <netinet/in.h> // struct sockaddr_in 
#include <fcntl.h>      // fcntl(() 

#define MAXDATA 64 // Tamano maximo del mensaje a enviar

// Variables GLOBALES:
// 'socket_fd' descriptor del socket
// 'transmitted' numero de peticiones enviadas
// 'received' numero de respuestas recibidas
// 'client_fd': Descriptor del socket cliente
int socketfd, clientfd, transmitted = 0, received = 0;
int padre; // PID del proceso padre

// Senal de interrupcion Ctrl + C
void INThandler(int);
// Funcion para despertar a los procesos hijos
void nop(){}

int main(int argc, char *argv[])
{
    // Variables LOCALES:
    // 'sockaddr_in' estructura para dominio internet. Se usa en bind(), sendto() y recvfrom().
    // 'long_client' tamano que tiene un sockaddr_in. Se usa en bind(), sendto() y recvfrom()
    struct sockaddr_in client;
    int long_client = sizeof(client);

    char data[MAXDATA]; // contiene un string de tamano MAXDATA (el mensaje)

    // 'port' numero de puerto en el que se establece el socket
    // 'hijo' PID del proceso hijo, que manejara los mensajes enviados y recibidos
    // 'dataSize' numero de bytes recibidos.
    int port = 0, hijo, dataSize = 0;

    signal(SIGINT, INThandler); // funcion captura señal Ctrl + C (SIGINT signal interrupt)
    
    // CONTROL DE ARGUMENTOS
    if (argc != 2)
    {
        printf("ERROR: numero de argumentos incorrecto.\nEJEMPLO: ./ping_noc_servidor [PUERTO]");
        exit(-1);
    }
    // Asignamos el puerto
    if ((port = atoi(argv[1])) < 1024) // ¿Es un peurto reservado? (<1024)
    {
        perror("ERROR: puerto reservado (<1024)");
        exit(-1);
    }

    // Inicializacion estructura sockaddr_in:    
    client.sin_family = AF_INET; // 'sin.family' indica IPv4 (siempre será AF_INET)
    client.sin_port = htons(port); // almacena el puerto al que se asigna el socket
    client.sin_addr.s_addr = htonl(INADDR_ANY); // almacena la direccion IP (INADDR_ANY asigna calquier IP)

    // CREACION del socket NOC: int socket(int dominio, int tipo, int protocolo);
    // 'dominio' AF_INET (IPv4)
    // 'tipo' SOCK_DGRAM soporta datagramas (NOC)
    // 'protocolo' 0 (corresponde a IP)
    // Si el descriptor que devuelve es menos a 0 se ha producido un error
    if ((socketfd = socket(AF_INET, SOCK_STREAM , 0)) < 0)
    {
        perror("ERROR: Error en la creacion del socket.");
        exit(-1);
    }

   // ASOCIACION del socket NOC: int bind(int socket, const struct sockaddr *address, int long);
    // 'socket' descriptor del socket
    // 'address' estructura con la direccion IP y puerto para asociar el socket (cast para convetirlo a un puntero const)
    // 'long' tamano en bytes de la estructura sockaddr client
    // El resultado devuelto por bind debe ser distinto de 0
    if (bind(socketfd, (const struct sockaddr *)& client, long_client) != 0)
    {
        perror("ERROR: Error en la asignacion de IP y puerto del socket.");
        close(socketfd);
        exit(-1);
    }
    // ESCUCHA de peticiones: int listen(int sockfd, int nPendientes);
    // 'sockfd': valor del descriptor del socket
    // 'nPendientes': define el tamano max de la cola de conexiones pendientes en el socket
    // Comprobamos que el resultado devuelto por listen sea distinto de 0
    if (listen (socketfd, 5) != 0)
    {
        perror("ERROR: Error en la la escucha de peticiones.");
        close(socketfd);
        exit(-1);
    }

    printf("Socket creado y asignado.\nEsperando una peticion ...\n");
    printf("# Log de enventos servidor #\n");

    // Espera de peticiones (buble infinito)
    while (1)
    {
        // ESTABLECIMIENTO DE LA CONEXIÓN:
        // int accept(int sockfd, struct sockaddr *addr, int long)
        // 'socket' descriptor del socket
        // 'address' estructura con la direccion IP y puerto para asociar el socket
        // 'long' tamano en bytes de la estructura sockaddr client
        // El resultado devuelto por bind debe ser distinto de 0
        if ((clientfd = accept(socketfd, (struct sockaddr *)& client, &long_client)) == -1)
        {
            perror("ERROR: Error al aceptar al cliente.");
            close(socketfd);
            exit(-1);
        }
        printf("CONEXION ESTABLECIDA (%s)\n", inet_ntoa(client.sin_addr));

        // Creacion de un proceso hijo
        if ((hijo = fork()) < 0)
        {
            perror("ERROR: Error al crear el proceso hijo.");
            close(socketfd);
            exit(-1);
        }
        // Si estamos en el proceso hijo 
        else if (hijo == 0)
        {
            // Asignamos la señal SIGCONT a la funcion nop
            signal(SIGCONT, nop); // 'SIGCONT' es enviada para reanudar un proceso 
            // Al crear un proceso hijo, los descriptores se duplican
            // cerramos el socket del servidor abierto, para no tener dos iguales
            close(socketfd);
            do {
                // Espera de respuesta echo. Comprobamos
                if ((dataSize = recv(clientfd, (char *)data, MAXDATA, 0)) > 0)
                {
                   printf("Mensaje recibido de (%s): %s\n", inet_ntoa(client.sin_addr), data); // Mostrar mensajes recibidos
                   sscanf(data, "respuesta ping %d", &received); // Extraemos el numero de peticion almacenandolo en received
                   sprintf(data, "icmp_seq=%d", received); // Mensaje de respuesta con el numero de peticion

                    // Confirmacion de la respuesta eco. Comprobamos que es correcta 
                    if (send(clientfd, data, MAXDATA, 0) < 0)
                    {
                        perror("ERROR: Error en el envio de la respuesta echo.");
                        close(socketfd);
                        exit(-1);
                    }
                    transmitted++;
                }
                else if (dataSize == -1)
                {
                    perror("ERROR: Error en la recepcion de los datos.");
                    close(clientfd);
                    exit(-1);
                }
            // Mientras haya datos que recibir, continuamos en el bucle.
            } while(dataSize > 0);
            printf("CONEXION LIBERADA (%s)\n", inet_ntoa(client.sin_addr));
            printf("\n----- Enviadas %d respuestas. -----\n", transmitted);
            exit(0);
        }
        // Estamos en el proceso padre 
        else
        // Obtenemos el PID del padre y cerramos el socket con el cliente porque este continua en el hijo
        padre = getpid();
        transmitted = 0;
        close(clientfd);
    }

    // En caso de que falle el buble se cierra el socket y se sale del programa 
    printf("ERROR: Se cerrara el programa.\n");
    close(socketfd);
    exit(-1);
}

// Funcion para capturar la señal Ctrl + C
void INThandler(int sig)
{
    char salir;
    // Si es el proceso padre 
    if (padre == getpid())
    {
        printf("\nQuieres salir del programa? [y/n] ");
        fflush (stdout);
        salir = getchar();
        // al salir cerramos los sockets y mandamos la señal SIGTERM a los procesos hijos
        if (salir == 'y' || salir == 'Y')
        {
            close(socketfd);
            close(clientfd);
            kill(0, SIGTERM);
        }
        // Si no, mandamos SIGCONT a los procesos hijos
        else
        {
            kill(0, SIGCONT);
        }
    }
    // Si es cualquiera de los hijos, lo pausamos a la espera del SIGCONT
    else
    {
        pause();
    }
}
