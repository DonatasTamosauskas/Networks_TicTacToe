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
#define MAX_SEND_RETRY_COUNT 10
#define BOARD_SIZE 3
#define TIME_BETWEEN_GAMES 1000

#define NEW_CONNECTION 10
#define NEW_DATA 20
#define DISCONNECTED 21

#define DEBUG 1

struct packet_data {
    int gameState;
    int enemyMove;
    int x;
    int y;
};

struct received {
    int fileDescriptor;
    int dataLength;
    struct packet_data *data;
};

struct game_state {
    int gameState;
    int client1;
    int client2;
};

void prepareAddrinfoHints(struct addrinfo *info);

void handleError(int errorCode, int errorType);

int getPortNumber(char port[]);

int bindToPort(struct addrinfo *ai, int *listener);

int handleConnections(int listener, fd_set *master, int *maxFd, struct received *data, int *gameRunning);

int handleNewConnection(int listener, fd_set *master, int *maxFd);

int handleExistingConnection(int i, fd_set *master, int *maxFd, struct received *data);

int sendData(int socketFd, struct packet_data *data, int timeout);

int executeGame(int connection_type, struct received *receivedData, struct game_state *gameState,
                int gameBoard[BOARD_SIZE][BOARD_SIZE]);

int addPlayer(struct received *receivedData, struct game_state *gameState);

void clearGameBoard(int gameBoard[BOARD_SIZE][BOARD_SIZE]);

int handlePlayerDisconnect(struct game_state *gameState, struct received *receivedData);

int handleGameSequence(struct game_state *gameState, struct received *receivedData, int gameBoard[BOARD_SIZE][BOARD_SIZE]);

int handleNewPlayer(struct game_state *gameState, struct received *receivedData);

int checkIfWon(int gameBoard[BOARD_SIZE][BOARD_SIZE], struct game_state *gameState);

void resetGame(struct game_state *gameState, struct received *receivedData, int gameBoard[BOARD_SIZE][BOARD_SIZE],
               int *connectionType);

int main() {

    int listener, maxFd, connectionType;
    int gameRunning;
    const char hostPort[5];
    struct addrinfo hints, *addrInfo;
    struct received receivedData;
    struct packet_data packetData;
    struct game_state gameState;
    int gameBoard[BOARD_SIZE][BOARD_SIZE];
    fd_set master;

    gameRunning = 1;
    memset(&receivedData, 0, sizeof(struct received));
    memset(&packetData, 0, sizeof(struct packet_data));
    memset(&gameState, 0, sizeof(struct game_state));
    receivedData.data = &packetData;
    FD_ZERO(&master);


    prepareAddrinfoHints(&hints);

    if ((getPortNumber(hostPort)) != 0) {
        handleError(errno, 1);
        return DEFAULT_ERROR_RETURN;
    }

    if (getaddrinfo(NULL, hostPort, &hints, &addrInfo) != 0) {
        handleError(errno, 2);
        return DEFAULT_ERROR_RETURN;
    }

    if ((bindToPort(addrInfo, &listener)) != 0) {
        handleError(errno, 3);
        return DEFAULT_ERROR_RETURN;
    }

    freeaddrinfo(addrInfo);

    if (listen(listener, MAX_CONNECTION_QUEUE) != 0) {
        handleError(errno, 4);
        return DEFAULT_ERROR_RETURN;
    }

    FD_SET(listener, &master);
    maxFd = listener;

    while (gameRunning) {

        if (gameState.gameState != 2) {
            connectionType = handleConnections(listener, &master, &maxFd, &receivedData, &gameRunning);
        } else {
            resetGame(&gameState, &receivedData, gameBoard, &connectionType);
        }

        executeGame(connectionType, &receivedData, &gameState, gameBoard);
        if (DEBUG) printf("Game state: %d\n", gameState.gameState);
    }

    return DEFAULT_RETURN;
}

/*
 * Desc:
 * Params:
 *
 * Returns:
 */
int executeGame(int connection_type, struct received *receivedData, struct game_state *gameState,
                int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    if (gameState->gameState == 0 && connection_type == NEW_CONNECTION) {
        clearGameBoard(gameBoard);
        return handleNewPlayer(gameState, receivedData);

    } else if (gameState->gameState == 1 && connection_type == NEW_DATA) {
        return handleGameSequence(gameState, receivedData, gameBoard);

    } else if (connection_type == DISCONNECTED) {
        return handlePlayerDisconnect(gameState, receivedData);
    }

    return -3;
}

void resetGame(struct game_state *gameState, struct received *receivedData, int gameBoard[BOARD_SIZE][BOARD_SIZE],
               int *connectionType) {
    gameState->gameState = 0;
    receivedData->fileDescriptor = gameState->client2;
    gameState->client2 = 0;
    *connectionType = NEW_CONNECTION;
    clearGameBoard(gameBoard);
    sleep(TIME_BETWEEN_GAMES);
}

/*
 * Desc: Function controls the main game sequence.
 * Params:
 *   gameState - structure with current game state information
 *   receivedData - data received from handleConnections function
 * Returns: -1 on error, 0 otherwise
 */
int
handleGameSequence(struct game_state *gameState, struct received *receivedData, int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    int winner;
    struct packet_data data;
    memset(&data, 0, sizeof(struct packet_data));

    data.gameState = 1;
    data.x = receivedData->data->x;
    data.y = receivedData->data->y;

    if (gameBoard[data.x][data.y] == 0) {
        gameBoard[data.x][data.y] = receivedData->fileDescriptor;
    } else {
        return DEFAULT_ERROR_RETURN;
    }

    winner = checkIfWon(gameBoard, gameState);
    if (winner == gameState->client1) {
        data.gameState = 2;
        data.enemyMove = 0;
        sendData(gameState->client1, &data, MAX_SEND_RETRY_COUNT);

        data.enemyMove = 1;
        sendData(gameState->client2, &data, MAX_SEND_RETRY_COUNT);
        gameState->gameState = 2;
        return DEFAULT_RETURN;

    } else if (winner == gameState->client2) {
        data.gameState = 2;
        data.enemyMove = 0;
        sendData(gameState->client2, &data, MAX_SEND_RETRY_COUNT);

        data.enemyMove = 1;
        sendData(gameState->client1, &data, MAX_SEND_RETRY_COUNT);
        gameState->gameState = 2;
        return DEFAULT_RETURN;

    } else if (winner == -1) {
        data.gameState = 2;
        data.enemyMove = 2;
        sendData(gameState->client1, &data, MAX_SEND_RETRY_COUNT);
        sendData(gameState->client2, &data, MAX_SEND_RETRY_COUNT);
        gameState->gameState = 2;
        return DEFAULT_RETURN;

    } else if (receivedData->fileDescriptor == gameState->client1) {
        data.enemyMove = 0;
        sendData(gameState->client2, &data, MAX_SEND_RETRY_COUNT);

        data.enemyMove = 1;
        sendData(gameState->client1, &data, MAX_SEND_RETRY_COUNT);
        return DEFAULT_RETURN;

    } else if (receivedData->fileDescriptor == gameState->client2) {
        data.enemyMove = 0;
        sendData(gameState->client1, &data, MAX_SEND_RETRY_COUNT);

        data.enemyMove = 1;
        sendData(gameState->client2, &data, MAX_SEND_RETRY_COUNT);
        return DEFAULT_RETURN;

    } else {
        return DEFAULT_ERROR_RETURN;
    }
}

/*
 * Desc: Checks if the current game board has been won by any client.
 * Params:
 *    gameBoard - current representation of the game board
 *    gameState - current game state
 * Returns: file descriptor of the winning client or 0, if no winner is present and -1 if draw
 */
int checkIfWon(int gameBoard[BOARD_SIZE][BOARD_SIZE], struct game_state *gameState) {
    int sumClient1, sumClient2, drawSum;
    int client1 = gameState->client1;
    int client2 = gameState->client2;

    // Vertical & draw
    for (int i = 0; i < BOARD_SIZE; i++) {
        sumClient1 = 0;
        sumClient2 = 0;
        drawSum = 0;

        for (int j = 0; j < BOARD_SIZE; j++) {
            sumClient1 += gameBoard[i][j] == client1;
            sumClient2 += gameBoard[i][j] == client2;

            drawSum += gameBoard[i][j] == client1;
            drawSum += gameBoard[i][j] == client2;
        }

        if (sumClient1 == BOARD_SIZE) return client1;
        if (sumClient2 == BOARD_SIZE) return client2;

        if (drawSum == BOARD_SIZE * BOARD_SIZE) return -1;
    }

    // Horizontal
    for (int i = 0; i < BOARD_SIZE; i++) {
        sumClient1 = 0;
        sumClient2 = 0;

        for (int j = 0; j < BOARD_SIZE; j++) {
            sumClient1 += gameBoard[j][i] == client1;
            sumClient2 += gameBoard[j][i] == client2;
        }

        if (sumClient1 == BOARD_SIZE) return client1;
        if (sumClient2 == BOARD_SIZE) return client2;
    }

    // Diagonal
    sumClient1 = 0;
    sumClient2 = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        sumClient1 += gameBoard[i][i] == client1;
        sumClient2 += gameBoard[i][i] == client2;
    }

    if (sumClient1 == BOARD_SIZE) return client1;
    if (sumClient2 == BOARD_SIZE) return client2;

    sumClient1 = 0;
    sumClient2 = 0;

    for (int i = 0; i < BOARD_SIZE; i++) {
        sumClient1 += gameBoard[i][BOARD_SIZE - i] == client1;
        sumClient2 += gameBoard[i][BOARD_SIZE - i] == client2;
    }

    if (sumClient1 == BOARD_SIZE) return client1;
    if (sumClient2 == BOARD_SIZE) return client2;

    return 0;
}


/*
 * Desc: Sets game board to initial state
 * Params:
 *    gameBoard - board to reset
 */
void clearGameBoard(int gameBoard[BOARD_SIZE][BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        memset(gameBoard[i], 0, sizeof(gameBoard[i]));
    }
}

/*
 * Desc: Function handles disconnections from clients in various game states.
 * Params:
 *    gameState - structure with current game state information
 *    receivedData - data received from handleConnections function
 * Returns:
 *    0 if handled correctly.
 *    -1 on error
 *    -2 on disconnection from non-client
 */
int handlePlayerDisconnect(struct game_state *gameState, struct received *receivedData) {
    if (gameState->gameState == 0) {
        gameState->client1 = 0;
        if (DEBUG) printf("Player #1 disconnected. No more connected players.\n");
        return DEFAULT_RETURN;

    } else if (gameState->gameState == 1) {
        if (gameState->client1 == receivedData->fileDescriptor) {
            gameState->client1 = gameState->client2;

        } else if (receivedData->fileDescriptor != gameState->client1 &&
                   receivedData->fileDescriptor != gameState->client2) {
            return DEFAULT_ERROR_RETURN;
        }

        gameState->gameState = 0;
        gameState->client2 = 0;

        struct packet_data exportData;
        memset(&exportData, 0, sizeof(struct packet_data));
        exportData.gameState = 0;
        if (sendData(gameState->client1, &exportData, MAX_SEND_RETRY_COUNT) == -1) return DEFAULT_ERROR_RETURN;

        if (DEBUG) printf("Player #2 disconnected.\n");
        return DEFAULT_RETURN;

    } else {
        return -2;
    }
}

/*
 * Desc: Handles connections of new players and their waiting in queue
 * Params:
 *    gameState - the current state of the game struct
 *    receivedData - struct that has the connecting players info
 * Returns: -1 on error, 0 otherwise
 */
int handleNewPlayer(struct game_state *gameState, struct received *receivedData) {
    int players = addPlayer(receivedData, gameState);
    struct packet_data exportData;
    memset(&exportData, 0, sizeof(struct packet_data));

    if (players < 0) {
        return DEFAULT_ERROR_RETURN;

    } else if (players == 1) {
        exportData.gameState = 0;
        if (sendData(gameState->client1, &exportData, MAX_SEND_RETRY_COUNT) == -1) return DEFAULT_ERROR_RETURN;

        if (DEBUG) printf("Player #1 added.\n");
        return DEFAULT_RETURN;

    } else if (players == 2) {
        exportData.gameState = 1;
        exportData.enemyMove = 0;
        exportData.x = -1;
        exportData.y = -1;
        if (sendData(gameState->client1, &exportData, MAX_SEND_RETRY_COUNT) == -1) return DEFAULT_ERROR_RETURN;

        exportData.enemyMove = 1;
        if (sendData(gameState->client2, &exportData, MAX_SEND_RETRY_COUNT) == -1) return DEFAULT_ERROR_RETURN;
        if (DEBUG) printf("Player #2 added\n");

        gameState->gameState = 1;
        return DEFAULT_RETURN;

    } else {
        return DEFAULT_ERROR_RETURN;
    }
}

/*
 * Desc: Adds connecting players to game state clients
 * Params:
 *    receivedData - struct that has the connecting player file descriptor
 *    gameState - struct to add the connections as clients
 * Returns:
 *    -1 - if error occured
 *    1 - if first client added
 *    2 - if second client added
 */
int addPlayer(struct received *receivedData, struct game_state *gameState) {
    if (gameState->client1 == 0) {
        gameState->client1 = receivedData->fileDescriptor;
        return 1;

    } else if (gameState->client2 == 0) {
        gameState->client2 = receivedData->fileDescriptor;
        return 2;

    } else {
        return -1;
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

    if (sentBits < sizeof(struct packet_data) && timeout > 0) {
        return (sendData(socketFd, data, timeout) == sizeof(struct packet_data)) - 1;
    }
    return (sentBits == sizeof(struct packet_data)) - 1;
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
int handleConnections(int listener, fd_set *master, int *maxFd, struct received *data, int *gameRunning) {
    fd_set readFds = *master;
    int newFd;

    if (select(*maxFd + 1, &readFds, NULL, NULL, NULL) < 0) {
        handleError(errno, 5);
        return DEFAULT_ERROR_RETURN;
    }

    for (int i = 0; i <= *maxFd; i++) {
        if (FD_ISSET(i, &readFds)) {
            if (i == listener) {

                if ((newFd = handleNewConnection(listener, master, maxFd)) < 0) {
                    return DEFAULT_ERROR_RETURN;

                } else {
                    data->fileDescriptor = newFd;
                    return NEW_CONNECTION;

                }

            } else {
                int receivedBits = handleExistingConnection(i, master, maxFd, data);
                if (receivedBits < 0) {
                    return DEFAULT_ERROR_RETURN;

                } else if (receivedBits == 0) {
                    return DISCONNECTED;

                } else {
                    return NEW_DATA;

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
 *   master - master set of all file descriptors
 *   maxFd - maximum file descriptor currently in use
 *   data - data buffer to be filled with packet data
 */
int handleExistingConnection(int incomingFd, fd_set *master, int *maxFd, struct received *data) {
    if (DEBUG) printf("Existing connection incoming.\n");
    int recv_bits = recv(incomingFd, data->data, sizeof(struct packet_data), 0);

    if (recv_bits < 0) {
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
int handleNewConnection(int listener, fd_set *master, int *maxFd) {
    if (DEBUG) printf("New connection incoming\n");
    struct socaddr_storage *remoteAddress;
    socklen_t addr_size = sizeof(remoteAddress);

    int newFd = accept(listener, (struct sockaddr *) remoteAddress, &addr_size);

    if (newFd == -1) {
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
    //TODO: Handle port reusing scenario

    struct addrinfo *curr;

    for (curr = ai; curr != NULL; curr = curr->ai_next) {

        *listener = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
        if (*listener < 0) continue;

        int bindError = bind(*listener, curr->ai_addr, curr->ai_addrlen);
        if (bindError < 0) {
            close(*listener);
            continue;
        }
        break;
    }

    if (curr == NULL) {
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
    info->ai_family = AF_UNSPEC;
    info->ai_socktype = SOCK_STREAM;
    info->ai_flags = AI_PASSIVE;
}

/*
 * Desc: Function gets the port number to be used for this instance.
 * Params:
 *   port - port number to be returned
 * Returns: Error code.
 */
int getPortNumber(char port[]) {
    for (int i = 0; i < 5; i++) {
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
    switch (errorType) {
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