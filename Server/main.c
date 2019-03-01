#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
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

#define PORT "9090"
#define DEFAULT_ERROR_RETURN 1;

void prepareAddrinfoHints(struct addrinfo *info);
int getPortNumber(char port[]);
void handleError(int errorCode, int errorType);
int bindToPort(struct addrinfo *ai, int *listener);

int main() {

    int errorCode, listener;
    const char hostPort[5];
    struct addrinfo hints, *addrInfo;

    prepareAddrinfoHints(&hints);

    if((errorCode = getPortNumber(hostPort)) != 0) {
        handleError(errorCode, 1);
        return DEFAULT_ERROR_RETURN;
    }

    if((errorCode = getaddrinfo(NULL, hostPort, &hints, &addrInfo)) != 0) {
        handleError(errorCode, 2);
        return DEFAULT_ERROR_RETURN;
    }

    if((errorCode = bindToPort(addrInfo, &listener)) != 0) {
        handleError(errorCode, 3);
        return DEFAULT_ERROR_RETURN;
    }

    return 0;
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

        int listener = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if(listener < 0) continue;

        int bindError = bind(listener, curr->ai_addr, curr->ai_addrlen);
        if(bindError < 0) {
            close(listener);
            continue;
        }

        break;
    }

    if(curr == NULL) {
        return -1;
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
 */
void handleError(int errorCode, int errorType) {
    switch(errorType) {
        case 1:
            printf("Unable to get port number. Error code: %d\n", errorCode);
            break;

        case 2:
            printf("Unable to get address info. Error code: %d\n", errorCode);
            printf("Error code explanation: %s\n", gai_strerror(errorCode));
            break;

        case 3:
            printf("Unable to bind to port. Error code: %d\n", errorCode);
            break;

        default:
            printf("Unknown error type %d. Error code: %d\n", errorType, errorCode);
    }
}


