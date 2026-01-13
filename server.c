#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int setup_server_socket(char *);

typedef struct ServerInfo {
    int socket;
    int max;
    fd_set master;
} ServerInfo;

int init_serverinfo(ServerInfo *si, char *portnumber) {
    if ((si->socket = setup_server_socket(portnumber)) < 0)
        return -1;  // Error in setting up server
    FD_ZERO(&si->master);
    FD_SET(si->socket, &si->master);
    si->max = si->socket;
    return 0;
}

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

typedef struct GameState {
    uint8_t status;
    uint8_t turn;
    uint8_t players[2];
    uint8_t board[9];
} GameState;

void init_game(GameState *gs) {
    gs->status = 'W';
    gs->turn = 0;
    memset(&gs->board, 0, sizeof(gs->board));
}

void update_board(GameState *gs, int row, int col) {
    gs->board[row*BOARDSIZE + col] = gs->turn;
}

int is_valid_move(GameState *gs, int row, int col) {
    // printf("%d %d\n", row, col);
    // printf("%d \n", gs->board[row*3 + col]);
    if (gs->board[row*3 + col] != 0) {
        printf("Move %d %d is invalid\n",row,col);
        return -1;
    }
    // printf("Move %d %d is valid\n", row, col);
    update_board(gs, row, col);
    return 0; 
}

void print_game(GameState *gs) {
    for(int row=0; row<3; row++) { 
        for(int col=0; col<3; col++) 
            printf("%d ", gs->board[row*3 + col]);
        printf("\n");
    }
}
//########################## PROTOCOL LAYER ###################################
typedef struct Message {
    uint8_t status;
    uint8_t turn;
    uint8_t player;
    uint8_t board[9];
} Message;

// Add error checking?
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

void initialize_player(GameState *gs, int client_socket) {
    if (!gs->players[0]) {
        gs->status = 'W';
        gs->turn = 0;
        gs->players[0] = client_socket;
        gs_to_msg(gs, client_socket); 
    }
    else {
        gs->status = 'A';
        gs->turn = 1;
        gs->players[1] = client_socket;
        gs_to_msg(gs, gs->players[1]); 
        gs_to_msg(gs, gs->players[0]); 
    }
}

void cleanup_client(ServerInfo *si, int fd) {
    printf("Client has disconnected.\n");
    FD_CLR(fd, &si->master);
    if (fd == si->max)
         // Disconnect client was the current max_socket
        while (si->max >= 0 && !FD_ISSET(si->max, &si->master)) 
            si->max--;
    close(fd);
}

int recieve_client_message(ServerInfo *si, GameState *gs, int fd) {
    uint32_t client_header;
    if (recv_all(fd, &client_header, sizeof(client_header)) < 1) {
        fprintf(stderr, "recv failed (%d).\n", errno);
        cleanup_client(si, fd);
        return -1;
    }

    // TODO: come back and double check clean up hasnt broke down the line.
    // Current overflow protection
    uint32_t len = ntohl(client_header);
    if (len > 1023) {
        printf("Client message was too long\n");
        // cleanup_client(si, fd);
        return -1;
    }

    char buf[1024];
    if (recv_all(fd, buf, len) < 1) {
        fprintf(stderr, "recv failed (%d).\n", errno);
        cleanup_client(si, fd);
        return -1;
    }

    // for (int i=0; i<len; i++) {
    //     printf("%d ", buf[i]);
    // }
    // printf("\n");

    int row = buf[0] - '1';
    int col = buf[1] - '1';
    is_valid_move(gs, row, col);

    print_game(gs);

    // if (!valid())
    //     resend message
    // else 
    //    send updated state to players
    return 0;
}

int test_message_to_client(int fd) {
    uint8_t msg[12];
    memset(&msg, 48, sizeof(msg));
    int turn = 0;
    int you = 1;
    msg[0] = 'W';
    msg[1] = turn;
    msg[2] = you;
    // uint8_t msg[] = {48, 48, 48, 48, 48, 48, 48, 48, 48}; 
    int msg_len = sizeof(msg);
    uint32_t server_header = htonl(msg_len);
    send_all(fd, &server_header, sizeof(server_header));
    send_all(fd, msg, msg_len);
    return 0;
}

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
                // Client disconnecting
                if (recieve_client_message(&si, &gs, fd) < 0) continue;
                test_message_to_client(fd);
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
