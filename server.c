// NETWORK LAYER - Works in isolation, exposes network apis.  
// GAME LAYER - Works in isolation, exposes game apis.
// PROTOCOL LAYER - Composes both game and network apis to create the program.
#include <stdio.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// -----------------------------NETWORK LAYER----------------------------------
/* Initial setup for the server, returns a socket listening to the local host 
 * at the port number provided. */
int server_listen(const char *port) {
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

int server_accept(int listen_sock) {
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
    
    char addrbuf[100], portbuf[100];
    getnameinfo((struct sockaddr*)&client_address, client_len, 
                addrbuf, sizeof(addrbuf), portbuf, sizeof(portbuf),
                NI_NUMERICHOST | NI_NUMERICSERV);
    printf("Client connected from address %s:%s\n", addrbuf, portbuf);
    
    return client_socket;
}

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

// ------------------------------GAME LAYER------------------------------------
#define BOARDSIZE 3

// Holds instance of game.
typedef struct game {
    // Might want an int array or malloc a char array here instead... unsure.
    int status;
    int turn;
    int *players[2];
    char *board;
} game;

// TODO: Initialize game correctly
void init_game(game *g) {
    g->board = malloc(sizeof(char)*BOARDSIZE*BOARDSIZE);
    g->turn = 0;
    g->status = 0;
    memset(&g->players, 0, sizeof(g->players));
}

// TODO: LOGIC IS BROKEN
/* Checks if the game is over and returns the winner player if so */
int gameover(char *b) {
    for (int i = 0; i < BOARDSIZE; i++) {
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

void mutate_game_state(void) { }

void reset_game(game *g) {
    memset(&g->board, 0, sizeof(g->board));
    g->num_players--;
    g->turn = 0;
}

// ----------------------------PROTOCOL  LAYER---------------------------------
// Defines the message protocol between the server and client 
typedef struct message {
    uint8_t status;
    uint8_t you; 
    uint8_t turn;
    uint8_t board[9];
} message;

// TODO: We wont need this 
char *serailize_board(char *board) {
    char *seralized = malloc(BOARDSIZE*BOARDSIZE + 1);
    int i;
    for (i = 0; i < BOARDSIZE*BOARDSIZE; i++)
        seralized[i] = '0' + board[i];
    seralized[i] = '\0';
    return seralized;
}

// TODO: We wont need this
// TODO: We want to eliminate strings, struct -> bytes -> bytes -> struct eventually
void send_board(int socket, game *g) {
    char payload[100];
    char *serial_killer = serailize_board(g->board);
    int payload_len = snprintf(payload, sizeof(payload), 
                               "STATUS=%c TURN=%d BOARD=%s", 
                               g->status, g->turn, serial_killer);

    uint32_t header = htonl(payload_len);
    send_all(socket, &header, sizeof header);
    send_all(socket, payload, payload_len);
    free(serial_killer);
}



// PROTOCOL & GAME LAYERS
// This will deal with recieving a player message and mutating game state.
void handle_client_req(game *g, int listen_sock, int fd, fd_set *master, int max) {
    char buf[5];
    int bytes = recv(fd, buf, sizeof buf-1, 0);
    buf[bytes] = '\0';
    if (bytes < 1) {
        printf("Client has disconnected.\n");
        reset_game(g);
        FD_CLR(fd, master);
        close(fd);
    }

    printf("%.*s\n", bytes, buf);
    int r, c;

    // Return straight away if invalid move.
    if (!is_valid(g->board, buf, &r, &c)) {
        printf("Move is invalid, try again\n");
        send_board(fd, g);
        return;
    }

    // Mutate the board with the players move
    g->board[r*BOARDSIZE + c] = g->turn;
    print_board(g->board);

    if (gameover(g->board)) g->status = 'F';        // Finished
    else g->turn = (g->turn == 1) ? 2 : 1;          // Switch player turn

    for (int i=0; i<=max; ++i) 
        if (i != listen_sock) 
            send_board(i, g);
}

// THIS IS BORKEN NOW
// TODO: Unsure about this
void initialize_player(game *g, int listen_sock, int client_socket, int max) {
    g->players;
    if (g->players == 1) 
        send_board(client_socket, g);
    else {
        g->turn = 1;    // ????
        g->status = 'S';  // >>>>?????
        send_board(client_socket, g);

        // I Think on the refactor we wont need this, we just send to all and 
        // the client will understand from the protocol we send, no unique id or 
        // number of players or random number 3 to dictate its knowledge
        // Start game for first player that connected
        for (int j=1; j<=max; ++j) 
            if (j != listen_sock && j != client_socket) 
                send_board(j, g);
    }
}

// Main loop for a game, should compose network and game calls together.
int run_server(int listen_sock) {
    game g;
    init_game(&g);

    // TODO: Consider moving fd set, max socket to a struct for cleaner mngmt
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

            if (fd == listen_sock) { 
                int new_client = server_accept(listen_sock);
                FD_SET(new_client, &master);
                if (new_client > max_sock) max_sock = new_client;
                initialize_player(&g, listen_sock, new_client, max_sock);
            }
            else 
                handle_client_req(&g, listen_sock, fd, &master, max_sock);
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

// Server setup
// Waits for two clients 
  // Client one connects 
    // Sends waiting for opponent 
  // Client two connects 
  
  // Sends to both clients that games is ready
  
// Both clients recieve game state 
  // Client one can only move 
  // Sends response to server 
// If incorrect, sends back to client one to try again 
// If correct, mutate interal gamestate 
// Both clients recieve new game state 

// Repeat until termination
// Reset game state 
// Disconnect both players.
