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

void prepareAddrinfoHints(struct addrinfo *info);
int getPortNumber(char port[]);
void handleError(int errorCode, int errorType);

int main() {

    int errorCode;
    char hostPort[4];
    struct addrinfo hints, *addrInfo;

    prepareAddrinfoHints(&hints);

    if((errorCode = getPortNumber(hostPort)) < 0) {
        handleError(errorCode, "PortNumber");
    }
    if(errorCode = getaddrinfo(NULL, ))

    return 0;
}

/*
 * Function sets addrinfo struct to use IP4/6, TCP and automatically assign host IP address.
 */
void prepareAddrinfoHints(struct addrinfo *info) {
    memset(info, 0, sizeof(*info));
    info -> ai_family = AF_UNSPEC;
    info -> ai_socktype = SOCK_STREAM;
    info -> ai_flags = AI_PASSIVE;
}

int getPortNumber(char port[]) {
    port = PORT;
    return 0;
}

/*
 * Handling errors.
 * Error type table:
 *   1. Port number
 *   2.
 */
void handleError(int errorCode, int errorType) {
    switch(errorType) {
        case 1:
            printf("Unable to get port number. Error code: %d", errorCode);
            break;

        default:
            printf("Unknown error type %d. Error code: %d", errorType, errorCode);
    }
}


