#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "9034"
#define SERVER_ADDRESS "localhost"
#define DEFAULT_ERROR_RETURN -1
#define MAX_HOSTNAME_LENGTH 200

void prepareAddrinfoHints(struct addrinfo *info);
int getPortNumber(char port[]);
void handleError(int errorCode, int errorType);
int connectToPort(struct addrinfo *ai, int *socketFd);
int getServerAddress(char address[], int address_length);


int main() {

    const char hostPort[5], hostname[MAX_HOSTNAME_LENGTH];
    struct addrinfo hints, *addrInfo;
    int socketFd;

    prepareAddrinfoHints(&hints);

    if((getPortNumber(hostPort)) != 0) {
        handleError(errno, 1);
        return DEFAULT_ERROR_RETURN;
    }

    if(getServerAddress(hostname, MAX_HOSTNAME_LENGTH) < 0) {

    }

    if(getaddrinfo(hostname, hostPort, &hints, &addrInfo) != 0) {
        handleError(errno, 2);
        return DEFAULT_ERROR_RETURN;
    }

    if(connectToPort(addrInfo, &socketFd) != 0) {
        handleError(errno, 3);
        return DEFAULT_ERROR_RETURN;
    }

}

/*
 * Desc: Function to get the remote address of the server.
 * Params:
 *   address - address to connect to, in text form
 *
 * Returns: 0 if okay, -1 if error occurred
 */
int getServerAddress(char address[], int address_length){
    for(int i = 0; i < address_length; i++){
        if(address[i] == 0) break;
        address[i] = SERVER_ADDRESS[i];
    }
    return 0;
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
 * Desc: Function gets the port number to be used for this instance.
 * Params:
 *   port - port number to be returned
 * Returns: Error code.
 */
//TODO: Implement this function
int getClientHostname(char hostname[MAX_HOSTNAME_LENGTH]) {
    read(stdin, &hostname, MAX_HOSTNAME_LENGTH);
    return 0;
}

/*
 * Desc: Function connects to first available address info
 * Params:
 *   ai - pointer to address info linked list
 *   listener - listener that has been binded to
 */
int connectToPort(struct addrinfo *ai, int *socketFd) {
    //TODO: Handle port reusing scenario

    struct addrinfo *curr;

    for(curr = ai; curr != NULL; curr = curr->ai_next) {

        *socketFd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if(*socketFd < 0) continue;

        int connectError = connect(*socketFd, curr->ai_addr, curr->ai_addrlen);
        if(connectError < 0) {
            close(*socketFd);
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
 * Desc: Function handles errors.
 * Params:
 *   errorCode - code provided by the method that threw the exception
 *   errorType - type of function that threw the code
 *
 * Error type table:
 *   1. Port number
 *   2. Get address info
 *   3. Connect to address & port
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
            printf("Unable to connect to address & port. Error code: %d\n", errorCode);
            break;

        default:
            printf("Unknown error type %d. Error code: %d\n", errorType, errorCode);
    }

    printf("Error code explanation: %s\n", gai_strerror(errorCode));
}
