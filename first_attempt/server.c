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
typedef struct gameState {
    // Might want an int array or malloc a char array here instead... unsure.
    int status;
    int turn;
    int players[2];
    char *board;
} gameState;

// TODO: Initialize game correctly
void init_game(gameState *game) {
    game->board = malloc(sizeof(char)*BOARDSIZE*BOARDSIZE);
    game->turn = 0;
    game->status = 0;
    memset(&game->players, 0, sizeof(game->players));
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

/* Chcks a coordinate for valid move 
 * Returns 0 for invalid, 1 for valid*/
int is_valid(char *b, int *bytes, int *r, int *c) {
    // -1 for the 0 idx offset. User will input 1,2 or 3, array idx are 0,1,2
    *r = bytes[1] - '0' - 1;
    *c = bytes[2] - '0' - 1;

    if (*r > BOARDSIZE || *r < BOARDSIZE || *c > BOARDSIZE || *c < BOARDSIZE) {
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

// Free the client socket we assigned for game->player
void reset_game(gameState *game, int fd) {
    memset(&game->board, 0, sizeof(game->board));
    // Cleaner way please 
    memset(&game->players, 0, BOARDSIZE*BOARDSIZE);
    // If i want to keep the other play i need to do the below, but also remake
    // Initialize to ensure we start a game correctly again as players[] is wrong
    // if (game->players[0] == fd) game->players[0] = 0;
    // else game->players[1] = 0;
    game->turn = 0;
}

// ----------------------------PROTOCOL  LAYER---------------------------------
// Defines the message protocol between the server and client 
typedef struct message {
    uint8_t status;
    uint8_t you; 
    uint8_t turn;
    uint8_t board[9];
} message;

void send_message(int socket, message *msg) {
    size_t message_len = sizeof(*msg);
    uint32_t header = htonl(message_len);
    send_all(socket, &header, sizeof header);
    send_all(socket, msg, message_len);
}

/* Game state to message converter*/
void convert_to_bytes(gameState *game, int client_socket) {
    message msg;
    msg.status = game->status;
    msg.turn = game->turn;

    // Assigning who a player is via socket
    if (game->players[0] == client_socket) msg.you = 1;
    else msg.you = 2;

    memcpy(msg.board, game->board, 9);
    send_message(client_socket, &msg);
}

// PROTOCOL & GAME LAYERS
// This will deal with recieving a player message and mutating game state.
int handle_client_req(gameState *game, int fd, fd_set *master) {

    // I think broken logic
    // Return immediatly if non player turn tried to make a move.
    // if (game->players[game->turn] != fd)
    //     return 0;

    uint32_t header;
    if (recv_all(fd, &header, sizeof header) < 1) {
        printf("Client has disconnected\n");
        reset_game(game, fd);
        FD_CLR(fd, master);
        close(fd);
        return -1;
    }

    uint32_t len = ntohl(header);
    uint8_t buf[len];
    if (recv_all(fd, buf, len) < 1) {
        printf("Client has disconnected\n");
        reset_game(game, fd);
        FD_CLR(fd, master);
        close(fd);
        return -1;
    }

    uint8_t r = buf[0], c = buf[1];
    printf("%d %d\n", r, c);
    // BROEKEN: Returns true always
    // Return straight away if invalid move.
    // if (!is_valid(game->board, buf, &r, &c)) {
    //     printf("Move is invalid, try again\n");
        // convert_to_bytes(game, fd);
        // return 0;
    // }

    // Mutate the board with the players move
    // game->board[r*BOARDSIZE + c] = game->turn;
    // print_board(game->board);

    // if (gameover(game->board)) game->status = 'F';        // Finished
    // else game->turn = (game->turn == 0) ? 1 : 0;          // Switch player turn

    // send to both players
    // for (int i=0; i<2; ++i) 
    //     convert_to_bytes(game, game->players[i]);

    return -1;
}

/* Initializes both players and starts the game once two players are present*/
void initialize_player(gameState *game, int client_socket) {
    if (!game->players[0]) {
        game->players[0] = client_socket;
        game->status = 'W';                            // Wait
        game->turn = 0;
        convert_to_bytes(game, client_socket);        
    }
    else {
        game->players[1] = client_socket;
        game->turn = 1;   
        game->status = 'S';                            // Start
        convert_to_bytes(game, game->players[1]);        
        convert_to_bytes(game, game->players[0]);        
    }
}

// Main loop for a game, should compose network and game calls together.
int run_server(int listen_sock) {
    gameState game;
    init_game(&game);

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
                initialize_player(&game, new_client);
            }
            else {
                // NOTE: Currently always returns 0
                int ret = handle_client_req(&game, fd, &master);
                if (ret < 0)
                    // No error handling implemented yet.
                    continue;
            }
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
