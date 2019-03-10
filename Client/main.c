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
#define BOARD_SIZE 3
#define DEFAULT_ERROR_RETURN -1
#define DEFAULT_RETURN 0
#define MAX_HOSTNAME_LENGTH 200
#define MAX_RETRY_COUNT 10
#define ADVERSARY_NBR 2
#define DEBUG 0

struct packet_data {
    int gameState;
    int enemyMove;
    int x;
    int y;
};

void prepareAddrinfoHints(struct addrinfo *info);
int getPortNumber(char port[]);
void handleError(int errorCode, int errorType);
int connectToPort(struct addrinfo *ai, int *socketFd);
int getServerAddress(char address[], int address_length);
int sendData(int socketFd, struct packet_data *data, int timeout);
int receiveData(int socketFd, struct packet_data *data);
int playMatch(int socketFd, int gameBoard[BOARD_SIZE][BOARD_SIZE]);
int playMove(int socketFd, int gameBoard[BOARD_SIZE][BOARD_SIZE]);
void displayGameBoard(struct packet_data *gameData, int gameBoard[BOARD_SIZE][BOARD_SIZE]);
void clearGameBoard(int gameBoard[BOARD_SIZE][BOARD_SIZE]);


int main() {

    const char hostPort[5], hostname[MAX_HOSTNAME_LENGTH];
    int gameBoard[BOARD_SIZE][BOARD_SIZE];
    struct addrinfo hints, *addrInfo;
    int socketFd, gameRunning;

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

    freeaddrinfo(addrInfo);
    gameRunning = 1;
    clearGameBoard(gameBoard);

    while(gameRunning) {
        playMatch(socketFd, gameBoard);
    }


}
/*
 * Desc:
 * Params:
 *
 * Returns:
 */
int playMatch(int socketFd, int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    struct packet_data game_data;
    memset(&game_data, 0, sizeof(struct packet_data));

    receiveData(socketFd, &game_data);
    if(DEBUG) printf("GameState: %d.\n", game_data.gameState);

    if(game_data.gameState == 0) {
        printf("Waiting for a second client to connect.\n");
        clearGameBoard(gameBoard);
        return DEFAULT_RETURN;

    } else if(game_data.gameState == 1) {
        if(game_data.enemyMove == 0) {
            system("clear\n");

            if(game_data.x >= 0 && game_data.y >= 0) gameBoard[game_data.x][game_data.y] = ADVERSARY_NBR;
            displayGameBoard(&game_data, gameBoard);

            printf("Your move. \n");
            playMove(socketFd, gameBoard);

        } else if (game_data.enemyMove == 1){
            gameBoard[game_data.x][game_data.y] = ADVERSARY_NBR + 1;
            displayGameBoard(&game_data, gameBoard);

            printf("Wait for your turn.\n");
            return DEFAULT_RETURN;
        }
    } else if(game_data.gameState == 2) {
        printf("Match concluded.\n");
        if(game_data.enemyMove == 0) printf("Concgradulations, you won!\n");
        if(game_data.enemyMove == 1) printf("Bummer, you lost :(\n");
        if(game_data.enemyMove == 2) printf("Whoa, it's a draw :O\n");

        clearGameBoard(gameBoard);
    }
    return DEFAULT_ERROR_RETURN;
}

/*
 * Desc:
 * Params:
 *
 * Returns:
 */
int playMove(int socketFd, int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    int x, y;
    char temp[200];
    struct packet_data data;
    memset(&data, 0, sizeof(struct packet_data));

    printf("Enter the vertical (Y) and then the horizontal (X) coordinates you'd like to play.\n");
    scanf("%s", temp);
    x = atoi(temp);
    scanf("%s", temp);
    y = atoi(temp);

    printf("%d", x);

    if(x >= 0 && x <=BOARD_SIZE - 1 && y >= 0 && y <= BOARD_SIZE - 1 && gameBoard[x][y] == 0) {
        data.x = x;
        data.y = y;
        return sendData(socketFd, &data, MAX_RETRY_COUNT);

    } else {
        printf("Please enter valid coordinates!\n");
        return playMove(socketFd, gameBoard);
    }
}

/*
 * Desc:
 * Params:
 *
 * Returns:
 */
void displayGameBoard(struct packet_data *gameData, int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    printf("  ");
    for(int i = 0; i < BOARD_SIZE; i++) printf(" %d", i);
    printf("\n  ");
    for(int i = 0; i < BOARD_SIZE; i++) printf("__");
    printf("\n");

    for(int i = 0; i < BOARD_SIZE; i++) {
        printf("%d|", i);
        for(int j = 0; j < BOARD_SIZE; j++) {

            if(gameBoard[i][j] == 0) printf("  ");
            else if(gameBoard[i][j] == ADVERSARY_NBR) printf(" X");
            else if(gameBoard[i][j] == ADVERSARY_NBR + 1) printf(" O");
        }
        printf("\n");
    }
}

/*
 * Desc: Sets game board to initial state
 * Params:
 *    gameBoard - board to reset
 */
void clearGameBoard(int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    for(int i = 0; i < BOARD_SIZE; i++) {
        memset(gameBoard[i], 0, sizeof(gameBoard[i]));
    }
}

/*
 * Desc: Function repeatedly tries to send all data until it's all sent or a timeout is reached.
 * Params:
 *   socketFd - socket file descriptor to be used for sending data
 *   data - packet_data to send
 *   timeout - number of retries to send data in case it's not sent
 * Returns: 0 if sent, -1 if error
 */
int sendData(int socketFd, struct packet_data *data, int timeout) {
    int sentBits = send(socketFd, data, sizeof(struct packet_data), 0);

    if(sentBits < sizeof(struct packet_data) && timeout > 0) {
        return (sendData(socketFd, data, timeout) == sizeof(struct packet_data)) -1;
    }
    return (sentBits == sizeof(struct packet_data)) -1;
}

/*
 * Desc:
 * Params:
 *   socketFd - socket file descriptor to listen for
 *   data - packet_data buffer to fill with received data
 * Returns: 0 if sent, -1 if error
 */
int receiveData(int socketFd, struct packet_data *data) {
    int receivedBits = recv(socketFd, data, sizeof(struct packet_data), 0);
    return (receivedBits == sizeof(struct packet_data)) -1;
}


/*
 * Desc: Function to get the remote address of the server.
 * Params:
 *   address - address to connect to, in text form
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
