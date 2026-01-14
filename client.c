// The client in C was never optimized as I planned on building the client in 
// python, I ended up prototyping it in C alongside the client and building it 
// in python afterwards... strange order haha.
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define STATUS_WAIT 'W'
#define STATUS_ACTIVE 'A'
#define STATUS_DISCONNECT 'D'
#define STATUS_TIE 'T'
#define STATUS_FINISH 'F'

/* Creates a TCP socket and connects to a server at the given host and port 
 * Returns the server socket on success and -1 on failure*/
int connect_to_server(const char *host, const char *port) {
    printf("Configuring host address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *server_address;
    if (getaddrinfo(host, port, &hints, &server_address)) {
        fprintf(stderr, "getaddrinfo failed (%d).\n", errno);
        return -1;
    }
    
    int server_socket = socket(server_address->ai_family, 
                               server_address->ai_socktype,
                               server_address->ai_protocol);
    if (server_socket < 0) {
        fprintf(stderr, "socket failed (%d).\n", errno);
        return -1;
    }

    if (connect(server_socket, server_address->ai_addr, server_address->ai_addrlen)) {
        fprintf(stderr, "connect failed (%d).\n", errno);
        return -1;
    }
    printf("Connected to %s:%s\n", host, port);
    freeaddrinfo(server_address);
    return server_socket;
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

/* Prints a 3 by 3 Tic-Tac-Toe board to stdout*/
void print_game(uint8_t *board) {
    for(int row=0; row<3; row++) { 
        for(int col=0; col<3; col++) 
            printf("%d ", board[row*3 + col]);
        printf("\n");
    }
}

/* Handles an incoming message from the server
 * Recieves a 4 byte header indicating the message length incoming next 
 * Recieves the message from the server 
 * Parses game status, turn, player and board 
 * Prints the board and message, and prompts the client for their turn
*/
int handle_server_req(int server_socket) {
    uint32_t server_header;
    if (recv_all(server_socket, &server_header, sizeof(server_header)) < 1) {
        fprintf(stderr, "recv failed (%d).\n", errno);
        return -1;
    }

    uint8_t buf[1024];
    uint32_t len = ntohl(server_header);
    if (recv_all(server_socket, buf, len) < 1) {
        fprintf(stderr, "recv failed (%d).\n", errno);
        return -1;
    }

    char status = buf[0];
    int turn = buf[1];
    int you = buf[2];

    uint8_t *board = &buf[3];

    printf("\n");
    printf("status: %c, turn: %d, you: %d\n", status, turn, you);
    print_game(board);
    printf("\n");

    if (status == STATUS_FINISH) {
        if (turn == you) {
            printf("Game over - You won!\n");
            return -1;
        }
        else {
            printf("Game over - You Lost!\n");
            return -1;
        }
    }

    if (status == STATUS_TIE) {
        printf("Game over - It was a tied!\n");
        return -1;
    }

    if (status == STATUS_DISCONNECT) {
        printf("Opponent Disconnected, you win by default!\n");
        return -1;
    }

    if (status == STATUS_WAIT) {
        printf("Waiting for an opponent to join.\n");
        return 0;
    }
    
    if (you == turn) {
        printf("Enter a message to the server \n");
        char input[1024];
        if (!fgets(input, sizeof input, stdin)) {}

        uint32_t header = htonl(strlen(input));
        send_all(server_socket, &header, sizeof header);
        send_all(server_socket, input, strlen(input));
    }
    else 
        printf("Waiting for opponent to make a move\n");

    return 0;
}

/* Entry point for the client, runs the main loop for the clients game*/
int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ./client <hostname> <portnumber>\n");
        return -1;
    }
    char *hostname = argv[1];
    char *portnumber = argv[2];

    int server_sock = connect_to_server(hostname, portnumber);

    fd_set master;
    FD_ZERO(&master);
    // FD_SET(0, &master);
    FD_SET(server_sock, &master);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;

    while (1) {
        fd_set read_fds = master;

        if (select(server_sock+1, &read_fds, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select failed (%d)\n", errno);
            return -1;
        }

        // may not use, instead reply to server inside the req itself
        // if (FD_ISSET(0, &read_fds)) {
        //     // User input
        // }

        if (FD_ISSET(server_sock, &read_fds)) {
            if (handle_server_req(server_sock) < 0) break;
            // Server request
        }
    }

    close(server_sock);
    return 0;
}
