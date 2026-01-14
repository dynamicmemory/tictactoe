#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// ############################### NETWORK LAYER ###############################
int setup_server_socket(char *);

typedef struct ServerInfo {
    int socket;
    int max;
    fd_set master;
} ServerInfo;

/* Initializes a ServerInfo struct which contains 
 *   - the server listening socket
 *   - the number of the largest socket number 
 *   - a set of file descriptors of sockets 
 * Returns: 0 on success and -1 if the server failed to launch*/
int init_serverinfo(ServerInfo *si, char *portnumber) {
    if ((si->socket = setup_server_socket(portnumber)) < 0)
        return -1;  // Error in setting up server
    FD_ZERO(&si->master);
    FD_SET(si->socket, &si->master);
    si->max = si->socket;
    return 0;
}

/* Sets up a listening TCP socket on the given port 
 * Returns: listen_socket - a socket file descriptor on success, -1 on fail*/
int setup_server_socket(char *portnumber) {
    printf("Setting up server address...");
    struct addrinfo hints;
    struct addrinfo *bind_addr;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(0, portnumber, &hints, &bind_addr)) {
        fprintf(stderr, "getaddrinfo failed (%d)", errno);
        return -1;
    }

    printf("Creating socket for the server...\n");
    int listen_socket = socket(bind_addr->ai_family,
                               bind_addr->ai_socktype,
                               bind_addr->ai_protocol); 

    if ((listen_socket < 0)) {
        fprintf(stderr, "socket failed (%d)", errno);
        return -1;
    }

    printf("Binding socket to localhost, %s...\n", portnumber);
    if (bind(listen_socket, bind_addr->ai_addr, bind_addr->ai_addrlen) < 0) {
        fprintf(stderr, "bind failed (%d)", errno);
        return -1;
    }
    freeaddrinfo(bind_addr);

    if (listen(listen_socket, 5) < 0) {
        fprintf(stderr, "listen failed (%d)", errno);
        return -1;
    }
    printf("listening for connections on port: %s...\n", portnumber);
    return listen_socket;
}

/* Accepts a new client connection and adds its to the fd_set, and updates the 
 * max socket in the serverInfo struct 
 * Returns: client_socket - a socket file descriptor on success and -1 on fail
*/
int accept_client(ServerInfo *si) {
    struct sockaddr client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_socket = accept(si->socket, 
                               (struct sockaddr*)&client_addr, 
                               &addr_len); 
    if (client_socket < 0) {
        fprintf(stderr, "accept failed (%d)\n", errno);
        return -1;
    }

    char buf[100], service[100]; 
    getnameinfo((struct sockaddr*)&client_addr, addr_len,
                buf, sizeof(buf), service, sizeof(service), 
                NI_NUMERICHOST | NI_NUMERICSERV);
    printf("Accepting a new client from %s:%s\n", buf, service);

    FD_SET(client_socket, &si->master);
    if (client_socket > si->max) si->max = client_socket;

    return client_socket;
}

/* Sends exactly 'len' bytes over a socket, handles partial sends to ensure 
 * full message buffer is sent.
 * Returns: the total bytes sent on success or -1 on fail.*/
ssize_t send_all(int socket, const void *buf, size_t len) {
    size_t total = 0;
    const char *p = buf;

    while (total < len) {
        ssize_t n = send(socket, p+total, len-total, 0);
        if (n < 1) return -1;
        total += n;
    }
    return total;
}

/* Recieves exactly 'len' bytes over a socket, handles partial reads to ensure 
 * full message is recieved into the buffer 
 * Returns: the total bytes read in on success or -1 on failure
 */
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

//############################# GAME LAYER #####################################
#define BOARDSIZE 3
#define STATUS_WAIT 'W'
#define STATUS_ACTIVE 'A'
#define STATUS_DISCONNECT 'D'
#define STATUS_TIE 'T'
#define STATUS_FINISH 'F'

typedef struct GameState {
    uint8_t status;
    uint8_t turn;
    int players[2];
    uint8_t board[9];
} GameState;

/* Initialise a new GameState struct, sets the status to WAIT, the turn to 0, 
 * and clears the board to all 0s*/
void init_game(GameState *gs) {
    gs->status = STATUS_WAIT;
    gs->turn = 0;
    memset(&gs->board, 0, sizeof(gs->board));
}

/* Updates the board at the specified row nad column with the current turn*/
void update_board(GameState *gs, int row, int col) {
    gs->board[row*BOARDSIZE + col] = gs->turn;
}

/* Switches the current turn from player 1 to 2 or visa versa*/ 
void update_turn(GameState *gs) {
    gs->turn = (gs->turn == 1) ? 2 : 1;
}

// This is some of the worst coding ive ever done... proud... NOW GET RID OF IT!
int is_gameover(GameState *gs) {
    for (int i=0; i<BOARDSIZE*BOARDSIZE; i++) {
        if (gs->board[i] == 0) 
            break;
        if (i == 8) {
            gs->status = STATUS_TIE;
            return 1;
        }
    }

    for (int i=0; i<BOARDSIZE*BOARDSIZE; i++) {
        // Rows
        if (i%BOARDSIZE == 0 && gs->board[i] == gs->turn && 
            gs->board[i+1] == gs->turn && gs->board[i+2] == gs->turn) {
            gs->status = STATUS_FINISH;
            return 1;
        }

        // Columns
        if (i < 3 && gs->board[i] == gs->turn && gs->board[i+3] == gs->turn &&
            gs->board[i+6] == gs->turn) {
            gs->status = STATUS_FINISH;
            return 1;
        }

        // Left diag
        if (i == 0 && gs->board[i] == gs->turn && gs->board[i+4] == gs->turn &&
            gs->board[i+8] == gs->turn) {
            gs->status = STATUS_FINISH;
            return 1;
        }
        if (i == 2 && gs->board[i] == gs->turn && gs->board[i+2] == gs->turn &&
            gs->board[i+4] == gs->turn) {
            gs->status = STATUS_FINISH;
            return 1;
        }
    }
    return 0;
}

/* Checks if a move is valid within board bounds and on an empty cell */
int is_valid_move(GameState *gs, int row, int col) {
    if (row > 2 || row < 0 || col > 2 || col < 0 || gs->board[row*3 + col] != 0) {
        printf("Move %d %d is invalid\n",row,col);
        return -1;
    }
    return 0; 
}

/* Resets the GameState to initial values for a new game*/
void reset_game(GameState *gs) {
    gs->status = STATUS_WAIT;
    gs->turn = 0;
    memset(&gs->players, 0, sizeof(gs->players));
    memset(&gs->board, 0, sizeof(gs->board));
}

/* Prints a 3 by 3 Tic-Tac-Toe board to stdout*/
void print_game(GameState *gs) {
    printf("\n");
    for(int row=0; row<3; row++) { 
        for(int col=0; col<3; col++) 
            printf("%d ", gs->board[row*3 + col]);
        printf("\n");
    }
    printf("\n");
}
//########################## PROTOCOL LAYER ###################################
#define MAXBUF 256

typedef struct Message {
    uint8_t status;
    uint8_t turn;
    uint8_t player;
    uint8_t board[9];
} Message;

// Add error checking? update: error checking not needed we do it on recv =)
void send_message(Message *msg, int socket) {
    size_t message_len = sizeof(*msg);
    uint32_t header = htonl(message_len);
    send_all(socket, &header, sizeof header);
    send_all(socket, msg, message_len);
}

/* Game state to message converter*/
void gs_to_msg(GameState *gs, int client_socket) {
    Message msg;
    msg.status = gs->status;
    msg.turn = gs->turn;

    // Assigning who a player is via socket
    if (gs->players[0] == client_socket) msg.player = 1;
    else msg.player = 2;

    memcpy(&msg.board, gs->board, 9);
    send_message(&msg, client_socket);
}

/* Adds a new client to the game. If they are the first player they will wait 
 * for an opponent, If they are the second player, sets the game to start and 
 * alerts both player to begin */
void initialize_player(GameState *gs, int client_socket) {
    if (!gs->players[0]) {
        gs->status = STATUS_WAIT;
        gs->turn = 0;
        gs->players[0] = client_socket;
        gs_to_msg(gs, client_socket); 
    }
    else {
        gs->status = STATUS_ACTIVE;
        gs->turn = 1;
        gs->players[1] = client_socket;
        gs_to_msg(gs, gs->players[1]); 
        gs_to_msg(gs, gs->players[0]); 
    }
}

/* Removes disconnected clients from the fd_set and closes its socket */
void cleanup_client(ServerInfo *si, int fd) {
    printf("Client has disconnected.\n");
    FD_CLR(fd, &si->master);
    if (fd == si->max)
         // Disconnect client was the current max_socket
        while (si->max >= 0 && !FD_ISSET(si->max, &si->master)) 
            si->max--;
    close(fd);
}

/* Handles a clients disconnection and resets gamestate to a new game, alerts 
 * opponent of the disconnection
 */
void handle_disconnect(GameState *gs, int disconnected_socket) {
    gs->status = STATUS_DISCONNECT;
    // -1 as turns are 1,2 and player idxs are 0,1
    if (disconnected_socket == gs->players[gs->turn-1]) 
        update_turn(gs);
    gs_to_msg(gs, gs->players[gs->turn-1]);
    reset_game(gs);
}

/* Parses a clients message and stores it in 'buf' 
 * Returns 0 on success and -1 on disconnection or malicious behaviour*/
int receive_message(ServerInfo *si, int fd, uint8_t *buf) {
    uint32_t client_header;
    if (recv_all(fd, &client_header, sizeof(client_header)) < 1) return -1;

    // Current overflow protection
    uint32_t len = ntohl(client_header);
    if (len > MAXBUF) {
        printf("Client message was too long\n");
        return -1;
    }

    if (recv_all(fd, buf, len) < 1) return -1;

    return 0;
}

/* Sends updated gamestate to all players*/
void broadcast_message(GameState *gs) {
    for (int i=0; i<2; i++) 
        gs_to_msg(gs, gs->players[i]);
}

// TODO: This is god, we dont like god around here, break it up on refactor
/* Handles a clients move request, validates and updates the game board, 
 * checks for game over, updates turn and broadcasts new state to all players*/
int handle_client_request(ServerInfo *si, GameState *gs, int fd) {
    uint8_t buf[MAXBUF];
    if (receive_message(si, fd, buf) < 0) {
        cleanup_client(si, fd);
        handle_disconnect(gs, fd);
        return -1;
    }

    int row = buf[0] - '1';
    int col = buf[1] - '1';

    // Could we do this more slicker?
    if (gs->turn == 0 || gs->players[gs->turn-1] != fd) {
        printf("Illegal turn violation\n");
        return -1;
    }

    if (is_valid_move(gs, row, col) < 0) {
        printf("Invalid move\n");
        // Resend to client
        gs_to_msg(gs, fd);
        return -1;
    }

    update_board(gs, row, col);

    if (is_gameover(gs)) {
        broadcast_message(gs);
        reset_game(gs);
        return 0;
    }
    
    update_turn(gs);
    print_game(gs);
    broadcast_message(gs);
    return 0;
}

/* Main server loop, initializes server, accepts clients, handles requests and 
 * maintains the fd_set of sockets connected.
*/
int run_server(char *portnumber) {
    ServerInfo si;
    GameState gs;
    Message m;

    if (init_serverinfo(&si, portnumber) < 0) return -1;
    init_game(&gs);
    print_game(&gs);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    while (1) {
        fd_set read_fds = si.master;
        if (select(si.max+1, &read_fds, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select failed (%d)\n", errno);
            return -1;
        }

        for (int fd=0; fd<=si.max; fd++) {
            if (!FD_ISSET(fd, &read_fds)) continue;

            // Client connecting to the server 
            if (fd == si.socket) {
                int client_socket = accept_client(&si);
                if (client_socket < 0) continue;         //  failed to connect
                initialize_player(&gs, client_socket);
            }
            else {
                if (handle_client_request(&si, &gs, fd) < 0) continue;
            }
        }
    }
    // Close all client connections + server socket
    close(si.socket);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: ./server <portnumber>\n");
        return -1;
    }

    run_server(argv[1]);
    return 0;
}
