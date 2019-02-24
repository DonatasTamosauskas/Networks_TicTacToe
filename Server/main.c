#include <stdio.h>

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

int main() {
    printf("Hello, World!\n");
    return 0;
}