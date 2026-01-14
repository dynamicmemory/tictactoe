import sys
import socket 
import struct

HEADER_BYTES:int = 4
STATUS_FINISH:str = 'F'
STATUS_WAIT:str = 'W'
STATUS_ACTIVE:str = 'A'
STATUS_TIE:str = 'T'
STATUS_DISCONNECT:str = 'D'


def connect_to_server(hostname:str, portnumber:int) -> socket.socket | None:
    """
    Connects to the hostname and portnumber provided and returns the socket

    Args:
        hostname - the address of the server to connect to.
        portnumber - the portnumber to connect to.

    returns:
        server - the socket connection for the server 
        None - if the connect failed.
    """
    print("Creating the client socket")
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM) 
    try:
        server.connect((hostname, portnumber))
    except socket.error as e:
        print(f"Connection failed: {e}")
        return None
    print(f"Connected to {hostname}:{portnumber}")
    return server 


def recv_all(server: socket.socket, size:int) -> bytes:
    """
    Ensures that all bytes sent from a socket are receieved

    Args:
        hostname - the address of the server to connect to.
        portnumber - the portnumber to connect to.

    returns:
        bytes - 
    """
    data = b''
    while len(data) < size:
        chunks = server.recv(size - len(data))
        if not chunks:
            raise ConnectionError("Server disconnected")
        data += chunks 
    return data


def recieve_message(server:socket.socket) -> tuple:
    """
    Recieves both the header and game message from the server and splits the 
    message up into the relevant game fields.

    Args: 
        server - socket connection to the server

    Returns: 
        status - The current status of the game 
        turn - Whose turn it is 
        you - The current client 
        board - a list of 9 values representing the tictactoe board
    """
    header = recv_all(server, HEADER_BYTES)
    (length,) = struct.unpack('!I', header)

    message = recv_all(server, length)
    status = chr(message[0])
    turn = message[1]
    you = message[2]
    board = list(message[3:])
    return status, turn, you, board 


def send_move(server: socket.socket, move:str) -> None:
    """ Sends the players move to the server for processing 

    Args:
        server - socket connection to the server
        move - the players move in the form of row col
    """

    msg = move.encode()
    header = struct.pack('!I', len(msg))
    server.sendall(header + msg)


def print_board(board:list) -> None:
    """
    Prints the board in a 3 by 3 matrix to the clients terminal

    Args: 
        board - a list containing the 9 squares and their values 
    """
    print()
    for row in range(3):
        for col in range(3):
            print(board[row*3+col], end=" ")
        print()
    print()


def main(hostname:str, portnumber:int) -> None:
    """ Runs the main loop of the game for the client 

    Args:
        hostname - the address of the server to connect to.
        portnumber - the portnumber to connect to.
    """ 
    server: socket.socket|None = connect_to_server(hostname, portnumber) 
    if server is None:
        return 

    while True:
        try:
            status, turn, you, board = recieve_message(server)
            print(f"status: {status}, turn: {turn}, you: {you}")
            print_board(board)

            if status == STATUS_FINISH:
                if you == turn:
                    print("Game over - You won!")
                    break
                else:
                    print("Game over - You Lost!")
                    break
            elif status == STATUS_DISCONNECT: 
                print("Opponent disconnected - You won!")
                break
            elif status == STATUS_TIE:
                print("Game over - It is a draw")
                break
            elif status == STATUS_WAIT:
                print("Waiting for an opponent to join")
            else:
                if turn == you:
                    print("Enter your move (row col, 1-3, eg 11)")
                    move = input(">> ")
                    send_move(server, move)
                else:
                    print("Waiting for opponents move")
         
        except ConnectionError as e:
            print(e)
            break

    server.close()


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("usage: python client <hostname> <portnumber>")
        sys.exit(1)
    # if not isinstance(sys.argv, int):
    #     print("portnumber must be an integer")
        # sys.exit(1)
    main(sys.argv[1], int(sys.argv[2]))
