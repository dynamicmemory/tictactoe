#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define BOARDSIZE 3

// Holds instance of game.
typedef struct game {
    char board[BOARDSIZE*BOARDSIZE];
    int turn;
    int status;
} game;

// -------------------------------GAME STUFF ----------------------------------
// These all need to move to a game section or struct or idk... somewhere.
char BOARD[BOARDSIZE*BOARDSIZE] = {0};
int NUM_PLAYERS = 0;
int TURN = 0;
char STATUS = 'N';

/* Checks if the game is over and returns the winner player if so */
int gameover(char *b) {
    for (int i = 0; i < BOARDSIZE; i++) {
        int row = i*BOARDSIZE;
        // Row 
        // if (row % BOARDSIZE == 0 && b[row] != 0 && b[row] == b[row+1] && b[row] == b[row+2])
        if (b[i*BOARDSIZE] != 0 && b[i*BOARDSIZE] == b[i*BOARDSIZE +1] && b[i*BOARDSIZE] == b[i*BOARDSIZE+2])
            return b[i];
            // Columns
        else if (b[i] != 0 && b[i] == b[i + BOARDSIZE] && b[i] == b[i + (2*BOARDSIZE)])
            return b[i];
            // right diagonal
        else if (i == 0 && b[i] != 0 && b[i] == b[i+4] && b[i] == b[i+8]) 
            return b[i];
            // left diagonal
        else if (i == 2 && b[i] != 0 && b[i] == b[i+2] && b[i] == b[i+4]) 
            return b[i];
        else continue;
        // TODO: Still gotta handle draws
    }
    return 0;
}

/* Chcks a coordinate for valid move */
int is_valid(char *b, char *bytes, int *r, int *c) {
    *r = bytes[0] - '0' - 1;
    *c = bytes[1] - '0' - 1;

    if (*r > BOARDSIZE && *c > BOARDSIZE) {
        fprintf(stderr, "input was not between 1 - 3.\n");
        return 0;
    }

    if (b[*r * BOARDSIZE + *c] != 0)
        return 0;
    return 1;
}

/* Debugging purposes, prints a board to teh cli */
void print_board(char *board) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%c", board[i*BOARDSIZE + j] + '0');
        }
        printf("\n");
    }
}

void reset_game() {
    memset(&BOARD, 0, sizeof(BOARD));
    NUM_PLAYERS--;
    TURN = 0;
}

// -------------------------------NETWORKING STUFF -----------------------------
ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;

    while (total < len) {
        ssize_t n = send(sock, p+total, len-total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
}

ssize_t recv_all(int sock, void *buf, size_t len) {
    size_t total = 0;
    char *p = buf;

    while (total < len) {
        ssize_t n = recv(sock, p+total, len-total, 0);
        if (n <= 0) return -1;
        total += n;
    }
    return total;
} 

char *serailize_board(char *board) {
    char *seralized = malloc(BOARDSIZE*BOARDSIZE + 1);
    int i;
    for (i = 0; i < BOARDSIZE*BOARDSIZE; i++)
        seralized[i] = '0' + board[i];
    seralized[i] = '\0';
    return seralized;
}

void send_board(int socket, char *board, int you) {
    char payload[100];
    char *serial_killer = serailize_board(board);
    int payload_len = snprintf(payload, sizeof(payload), 
                               "STATUS=%c YOU=%d TURN=%d BOARD=%s", 
                               STATUS, you, TURN, serial_killer);

    uint32_t header = htonl(payload_len);
    send_all(socket, &header, sizeof header);
    send_all(socket, payload, payload_len);
    free(serial_killer);
}

/* Initial setup for the server, returns a socket listening to the local host 
 * at the port number provided. */
int server_listen(const char *port) {
    // Server stuff
    printf("Configuring server address...\n");
    struct addrinfo hints;
    struct addrinfo *bind_address;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(0, port, &hints, &bind_address)) {
        fprintf(stderr, "getaddrinfo failed (%d).\n", errno);
        return -1;
    }

    printf("Creating a socket for clients to connect too...\n");
    int socket_listen = socket(bind_address->ai_family, 
                               bind_address->ai_socktype,
                               bind_address->ai_protocol);
    if (socket_listen < 0) {
        fprintf(stderr, "socket failed (%d).\n", errno);
        return -1;
    }

    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind failed (%d).\n", errno);
        return -1;
    }

    freeaddrinfo(bind_address);

    printf("Server listening for new connections...\n");
    if (listen(socket_listen, 2) < 0) {
        fprintf(stderr, "listen failed (%d).\n", errno);
        return -1;
    }

    return socket_listen;
}

int server_accept(int listen_sock, fd_set *master, int *max) {
    // Setup the new client
    printf("New client connected, setting up socket...\n");
    struct sockaddr client_address;
    socklen_t client_len = sizeof(client_address);
    int client_socket = accept(listen_sock, 
                               (struct sockaddr*)&client_address, &client_len);

    if (client_socket < 0) {
        fprintf(stderr, "accept failed (%d).\n", errno);
        return -1;
    }
    // Add the client to the master socket set
    FD_SET(client_socket, master);
    if (client_socket > *max) *max = client_socket;
    NUM_PLAYERS++;

    char addrbuf[100], portbuf[100];
    getnameinfo((struct sockaddr*)&client_address, client_len, 
                addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf),
                NI_NUMERICHOST | NI_NUMERICSERV);
    printf("Client connected from address %s:%s\n", addrbuf, portbuf);

    // POSSIBLE SPLIT OF FUNCTION HERE
    // Set up game for client 
    if (NUM_PLAYERS == 1) 
        send_board(client_socket, BOARD, NUM_PLAYERS);
    else {
        TURN = 1;
        STATUS = 'S';    // Start
        send_board(client_socket, BOARD, NUM_PLAYERS);

        // Start game for first player that connected
        for (int j = 1; j <= *max; ++j) 
            if (j != listen_sock && j != client_socket) 
                send_board(j, BOARD, 3);
    }
    return 0;
}

void handle_client_req(int listen_sock, int fd, fd_set *master, int max) {
    char buf[5];
    int bytes = recv(fd, buf, sizeof buf-1, 0);
    buf[bytes] = '\0';
    if (bytes < 1) {
        printf("Client has disconnected.\n");
        reset_game();
        FD_CLR(fd, master);
        close(fd);
    }

    printf("%.*s\n", bytes, buf);
    int r, c;

    // Return straight away if invalid move.
    if (!is_valid(BOARD, buf, &r, &c)) {
        printf("Move is invalid, try again\n");
        send_board(fd, BOARD, 3);
        return;
    }

    // Mutate the board with the players move
    BOARD[r*BOARDSIZE + c] = TURN;
    print_board(BOARD);

    if (gameover(BOARD)) STATUS = 'F';        // Finished
    else TURN = (TURN == 1) ? 2 : 1;          // Switch player turn
    
    // send_board(fd, BOARD, 3);                 // Send to player whose turn it is

    for (int i=0; i<=max; ++i) 
        if (i != listen_sock) 
            send_board(i, BOARD, 3);
}

int run_server(int listen_sock) {
    fd_set master;
    FD_ZERO(&master);
    FD_SET(listen_sock, &master);
    int max_sock = listen_sock;

    while (1) { 
        fd_set read_fds = master;
        if (select(max_sock+1, &read_fds, 0, 0, 0) < 0) {
            fprintf(stderr, "select failed (%d)\n", errno);
            return -1;
        }

        for (int fd=0; fd<=max_sock; fd++) {
            if (!FD_ISSET(fd, &read_fds)) continue;

            if (fd == listen_sock) 
                server_accept(listen_sock, &master, &max_sock);
            else 
                handle_client_req(listen_sock, fd, &master, max_sock);
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: ./server <portnumber>");
        return 1;
    }

    int socket_listen = server_listen(argv[1]);
    if (socket_listen < 0) {
        printf("server_listen failed to return a socket.\n");
        return 1;
    }

    int server_exit = run_server(socket_listen);
    if (server_exit < 0) {
        printf("run_server crashed");
        return 1;
    }
    return 0;
}
