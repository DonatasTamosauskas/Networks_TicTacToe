#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
/*
 * Game states:
 *  1. Server is waiting for a client
 *  2. First client connects, server sends him a greeting
 *  3. First client keeps waiting for a second client
 *   a) If the first client disconnects return to state 1
 *  3. Second client connects
 *   a) If any of the clients disconnect, return to state 3
 *   b) If another client tries to connect send him an error message / put into queue
 *  4. Game starts, first client/random client begins
 *  5. Game continues until end
 *  5. Both clients get a choice to continue playing or to leave
 *   a) If clients continue playing, then return to state 4
 *   b) If one of the clients quits, then return client to state 3
 */

#define DEFAULT_ERROR_RETURN 1
#define DEFAULT_RETURN 0

#define PORT "9034"
#define MAX_CONNECTION_QUEUE 20
#define RECEIVING_BUFFER_SIZE 255

struct packet_data {
    int fileDescriptor;
    int dataLength;
    char buffer[RECEIVING_BUFFER_SIZE];
};

void prepareAddrinfoHints(struct addrinfo *info);
void handleError(int errorCode, int errorType);
int getPortNumber(char port[]);
int bindToPort(struct addrinfo *ai, int *listener);
int handleConnections(int listener, fd_set *master, int *maxFd, struct packet_data *data, int *gameRunning);
int handleNewConnection(int listener, fd_set *master, int *maxFd);
int handleExistingConnection(int i, fd_set *master, int *maxFd, struct packet_data *data);

int main() {

    int listener, maxFd;
    int gameRunning, receivedBits = 0;
    const char hostPort[5];
    struct addrinfo hints, *addrInfo;
    struct packet_data data;
    fd_set master;

    gameRunning = 1;
    memset(&data, 0, sizeof(struct packet_data));
    FD_ZERO(&master);


    prepareAddrinfoHints(&hints);

    if((getPortNumber(hostPort)) != 0) {
        handleError(errno, 1);
        return DEFAULT_ERROR_RETURN;
    }

    if(getaddrinfo(NULL, hostPort, &hints, &addrInfo) != 0) {
        handleError(errno, 2);
        return DEFAULT_ERROR_RETURN;
    }

    if((bindToPort(addrInfo, &listener)) != 0) {
        handleError(errno, 3);
        return DEFAULT_ERROR_RETURN;
    }

    freeaddrinfo(addrInfo);

    if(listen(listener, MAX_CONNECTION_QUEUE) !=0){
        handleError(errno, 4);
        return DEFAULT_ERROR_RETURN;
    }

    FD_SET(listener, &master);
    maxFd = listener;

    while(gameRunning) {
        printf("%d\n", handleConnections(listener, &master, &maxFd, &data, &gameRunning));
        printf("%s\n", data.buffer);
    }


    return DEFAULT_RETURN;
}

/*
 * Desc: Handles connections: accepts new ones, closes disconnected and receives data from existing ones.
 * Params:
 *   listener - file descriptor of listener socket
 *   master - master file descriptor set of all sockets
 *   buffer - char buffer for receiving data from an incoming packet
 *   gameRunning - state of game loop
 * Returns:
 *   DEFAULT_ERROR_RETURN - if error occurred
 *   10 - if a new connection was handled
 *   20 - if data was received from an existing connection
 *   21 - if an existing connection disconnected
 */
int handleConnections(int listener, fd_set *master, int *maxFd, struct packet_data *data, int *gameRunning) {
    fd_set readFds = *master;

    if(select(*maxFd + 1, &readFds, NULL, NULL, NULL) < 0) {
        handleError(errno, 5);
        return DEFAULT_ERROR_RETURN;
    }

    for(int i = 0; i <= *maxFd; i++){
        if(FD_ISSET(i, &readFds)) {
            if(i == listener) {

                if(handleNewConnection(listener, master, maxFd) < 0) {
                    return DEFAULT_ERROR_RETURN;

                } else {
                    return 10;

                }

            } else {
                int receivedBits = handleExistingConnection(i, master, maxFd, data);
                if(receivedBits < 0) {
                    return DEFAULT_ERROR_RETURN;

                } else if(receivedBits == 0) {
                    return 21;

                } else {
                    return 20;

                }
            }
        }
    }

    return DEFAULT_ERROR_RETURN;
}

/*
 * Desc: Handles data from existing connections: transfers incoming data from packets to buffer and terminates
 *       disconnected connections.
 * Params:
 *   incomingFd - the file descriptor of connection from which the data is coming
 *   maxFd - maximum file descriptor currently in use
 *   buffer - buffer to be filled with packet data
 */
int handleExistingConnection(int incomingFd, fd_set *master, int *maxFd, struct packet_data *data){
    printf("Existing connection incoming.\n");
    int recv_bits = recv(incomingFd, data->buffer, RECEIVING_BUFFER_SIZE, 0);

    if(recv_bits < 0) {
        handleError(errno, 7);
        close(incomingFd);
        FD_CLR(incomingFd, master);
        return DEFAULT_ERROR_RETURN;

    } else if (recv_bits == 0) {
        //TODO: Change the maxFD to be actual maxFD
        data->fileDescriptor = incomingFd;
        data->dataLength = 0;

        close(incomingFd);
        FD_CLR(incomingFd, master);
        return 0;

    } else {
        data->fileDescriptor = incomingFd;
        data->dataLength = recv_bits;
        return recv_bits;
    }
}

/*
 * Desc: Accepts a new connection and assigns a new file descriptor to it.
 * Params:
 *   listener - listener file descriptor
 *   master - the master set of all active file descriptors
 *   maxFd - the maximum file descriptor currently in use
 * Returns: -1 if error, new file descriptor otherwise.
 */
int handleNewConnection(int listener, fd_set *master, int *maxFd){
    printf("New connection incoming\n");
    struct socaddr_storage *remoteAddress;
    socklen_t addr_size = sizeof(remoteAddress);

    int newFd = accept(listener, (struct sockaddr *) remoteAddress, &addr_size);

    if(newFd == -1) {
        handleError(errno, 6);
        return DEFAULT_ERROR_RETURN;
    } else {
        FD_SET(newFd, master);
        if (newFd > *maxFd) *maxFd = newFd;
        return newFd;
    }
}

/*
 * Desc: Function binds to first available address info
 * Params:
 *   ai - pointer to address info linked list
 *   listener - listener that has been binded to
 */
int bindToPort(struct addrinfo *ai, int *listener) {

    struct addrinfo *curr;

    for(curr = ai; curr != NULL; curr = curr->ai_next) {

        *listener = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if(listener < 0) continue;

        int bindError = bind(*listener, curr->ai_addr, curr->ai_addrlen);
        if(bindError < 0) {
            close(*listener);
            continue;
        }
        break;
    }

    if(curr == NULL) {
        return -1;
    } else {
        return 0;
    }
}

/*
 * Desc: Function sets addrinfo struct to use IP4/6, TCP and automatically assign host IP address.
 * Params:
 *   info - addrinfo struct used for preparing data
 */
void prepareAddrinfoHints(struct addrinfo *info) {
    memset(info, 0, sizeof(struct addrinfo));
    info -> ai_family = AF_UNSPEC;
    info -> ai_socktype = SOCK_STREAM;
    info -> ai_flags = AI_PASSIVE;
}

/*
 * Desc: Function gets the port number to be used for this instance.
 * Params:
 *   port - port number to be returned
 * Returns: Error code.
 */
int getPortNumber(char port[]) {
    for(int i = 0; i < 5; i++){
        port[i] = PORT[i];
    }
    return 0;
}

/*
 * Desc: Function handles errors.
 * Params:
 *   errorCode - code provided by the method that threw the exception
 *   errorType - type of function that threw the code
 *
 * Error type table:
 *   1. Port number
 *   2. Get address info
 *   3. Bind to port
 *   4. Listen for connection
 *   5. Connection handler
 *      6. New connection handler
 *      7. Existing connection handler
 *
 */
void handleError(int errorCode, int errorType) {
    switch(errorType) {
        case 1:
            printf("Unable to get port number. Error code: %d\n", errorCode);
            break;

        case 2:
            printf("Unable to get address info. Error code: %d\n", errorCode);
            break;

        case 3:
            printf("Unable to bind to port. Error code: %d\n", errorCode);
            break;

        case 4:
            printf("Unable to listen. Errno: %d\n", errorCode);
            break;

        case 5:
            printf("Unable to handle an incoming connection. Errno: %d\n", errorCode);
            break;

        case 6:
            printf("Unable to accept a new connection. Errno: %d\n", errorCode);
            break;

        case 7:
            printf("Unable to handle data from an existing connection. Errno: %d\n", errorCode);
            break;

        default:
            printf("Unknown error type %d. Error code: %d\n", errorType, errorCode);
    }

    printf("Error code explanation: %s\n", gai_strerror(errorCode));
}



