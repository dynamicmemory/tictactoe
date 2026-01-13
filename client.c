#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

int connect_to_server(const char *host, const char *port) {
    printf("Configuring host address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *server_address;
    if (getaddrinfo(host, port, &hints, &server_address)) {
        fprintf(stderr, "getaddrinfo failed (%d).\n", errno);
        return 1;
    }
    
    int server_socket = socket(server_address->ai_family, 
                               server_address->ai_socktype,
                               server_address->ai_protocol);
    if (server_socket < 0) {
        fprintf(stderr, "socket failed (%d).\n", errno);
        return 1;
    }

    if (connect(server_socket, server_address->ai_addr, server_address->ai_addrlen)) {
        fprintf(stderr, "connect failed (%d).\n", errno);
        return 1;
    }
    printf("Connected to %s:%s\n", host, port);
    freeaddrinfo(server_address);
    return server_socket;
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

void print_game(uint8_t *board) {
    for(int row=0; row<3; row++) { 
        for(int col=0; col<3; col++) 
            printf("%d ", board[row*3 + col]);
        printf("\n");
    }
}

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
    // printf("%c %c %c\n", buf[0], buf[1], buf[2]);
    // printf("%c %c %c\n", buf[3], buf[4], buf[5]);
    // printf("%c %c %c\n", buf[6], buf[7], buf[8]);
    // printf("\n");

    // for (int i=0; i<len; i++){
    //     printf("%c ",board[i]);
    // }
    print_game(board);
    printf("\n");

    printf("Enter a message to the server \n");
    char input[1024];
    if (!fgets(input, sizeof input, stdin)) {}

    uint32_t header = htonl(strlen(input));
    send_all(server_socket, &header, sizeof header);
    send_all(server_socket, input, strlen(input));

    return 0;
}

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
    FD_SET(0, &master);
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
        if (FD_ISSET(0, &read_fds)) {
            // User input
        }

        if (FD_ISSET(server_sock, &read_fds)) {
            if (handle_server_req(server_sock) < 0) break;
            // Server request
        }
    }

    close(server_sock);
    return 0;
}
