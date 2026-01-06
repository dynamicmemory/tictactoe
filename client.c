#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#define BOARDSIZE 3

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

/* Debugging purposes, prints a board to teh cli */
void print_board(char *board) {
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%d", board[i*BOARDSIZE + j] - '0');
        }
        printf("\n");
    }
}

int server_socket(const char *host, const char *port) {
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

    printf("Connecting to server...\n");
    if (connect(server_socket, server_address->ai_addr, server_address->ai_addrlen)) {
        fprintf(stderr, "connect failed (%d).\n", errno);
        return 1;
    }
    freeaddrinfo(server_address);
    return server_socket;
}

int handle_server_req(int server_sock) {
    // RECIEVING SERVER MESSAGE
    char buf[100];

    uint32_t header;
    recv_all(server_sock, &header, sizeof header);
    if (header < 1) {
        printf("Server has disconnected\n");
        return -1;
    }

    uint32_t len = ntohl(header);
    if (recv_all(server_sock, buf, len) < 1) {
        printf("Server has disconnected\n");
        return -1;
    }

    // PARSING GAME PROTOCOL FROM MESSAGE
    // Game protocol message from the server
    printf("%.*s\n", len, buf);

    // STATUS=x PLAYERS=x YOU=x TURN=x BOARD=x
    int ecount = 0;
    char status;
    int you;
    int turn;
    char *board;

    for (int j=0; j<len; ++j) {
        if (buf[j] == '='){
            ++ecount;
            if (ecount == 1) {
                status = buf[++j];
                continue;
            }

            if (ecount == 2) {
                int temp = buf[++j] - '0';
                if (temp == 3) {
                    printf("buffer sent turn is 3 so %d with be %d\n", you, you);
                }
                else 
                    you = temp;
                continue;
            }
            if (ecount == 3) {
                turn = buf[++j] - '0';
                continue;
            }
            if (ecount == 4) {
                board = &buf[++j];
                printf("%s", board);
                break;
            }
        }
    }

    // CHECKING GAME STATE FROM REIVED SERVER MESSAGE
    // Only display board once both players have joined and game has started
    if (status != 'N') {
        printf("\n");
        print_board(board);
        printf("\n");
    }

    // IF GAME OVER; END ELSE; RETURN INPUT TO SERVER FOR MOVE.
    if (status == 'F') {
        if (turn == you) 
            printf("You won\n");
        else 
            printf("You lost\n");
        return 0;
    }
    else {
        if (turn == you) {
            printf("Enter your move <row><col>\n");
            char input[10];
            if (!fgets(input, sizeof input, stdin)) {}
            input[strcspn(input,"\n")] = '\0';
            send_all(server_sock, input, strlen(input));
        }
        else {
            // wait for turn 
        }
    }
    return 0;
}

int run_client(int server_sock) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server_sock, &reads);
    FD_SET(0, &reads);

    // struct timeval timeout;
    // timeout.tv_sec = 0;
    // timeout.tv_usec = 100000;

    while (1) {
        if (select(server_sock+1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select failed (%d).\n", errno);
            return -1;
        }

        if (FD_ISSET(0, &reads)) {
            int i = 0;
        }

        // Recieved server message 
        if (FD_ISSET(server_sock, &reads))
            handle_server_req(server_sock);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: ./client <hostname> <portnumber>\n");
        return -1;
    }
    char *hostname = argv[1];
    char *portnumber = argv[2];

    int server_sock = server_socket(hostname, portnumber);
    int exit_status = run_client(server_sock);

    if (exit_status < 0) {
        printf("run_client exiting violently\n");
        return 1;
    }

    close(server_sock);
    return 0;
}
