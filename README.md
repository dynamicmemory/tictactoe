# Networked Tic-Tac-Toe (C & Python)

This repository contains a **Somewhat finished** networked Tic-Tac-Toe implementation with a C server, C client, and a Python client.

The goal of this project is **not** to ship a commercial game.  
The goal is to provide a **working, cross-language example of network server architecture**:

- Socket lifecycle management
- Event-loop–driven servers (`select`)
- Binary protocol design
- Server-authoritative game state
- Cross-language client compatibility (C and Python)

---

## Current State

### C Server
- Handles multiple clients using `select`
- Binary protocol for sending game state
- Event loop isolated in `run_server`
- Game state management via `GameState` struct
- Fully functional Tic-Tac-Toe logic with turn enforcement

### C Client
- Connects to server using TCP
- Sends moves as two-character strings (`"rowcol"`)
- Receives game state as a **binary message** with a 4-byte header

### Python Client
- Connects to the same server using TCP
- Follows the **same binary protocol** as the C client
- Implements a read–eval–send loop
- Prints the board to terminal and prompts for moves
- Fully compatible with server

---

## Protocol

The protocol is **binary** with a **4-byte big-endian length header** followed by a payload:

[4-byte big-endian length][payload]

The payload consists of:

- 1 byte: `status` (`W`, `A`, `F`, `T`, `D`)
- 1 byte: `turn` (1 or 2)
- 1 byte: `player` (1 or 2)
- 9 bytes: `board` (values 0–2 for empty / player markers)

Moves sent from client to server are **2-byte ASCII strings**:  
`"11"` → row 1, column 1  

---

## Build

### C Server
```sh
gcc -Wall -Wextra -o server server.c

### C Client 
```sh
gcc -Wall -Wextra -o client client.c
```

### Python Client
Requires Python 3.10+ (for `socket | None` typing)
```sh
python3 client.py <hostname> <portnumber>
```

## RUN 

Start the server: 
```sh 
./server 8080
```

Connect clients:
```sh 
./client localhost 8080 
```

Python client:
```sh 
python3 client.py localhost 8080 
```

Two clients are expected.

## Architecture

High-level structure:

### Transport
- `setup_server_socket` / `connect_to_server`

### Event Loop
- `run_server` (server-side only)

### Connection Handling
- `accept_client` / `handle_client_request`

### Game Logic
- `GameState` struct (C)
- Python client mirrors server state to render board

**Key points:**
- Event loop does **not** know game rules
- Server is authoritative over game state
- Clients are treated as **untrusted input sources**
- Fully functional binary protocol ensures cross-language communication

---

## Why This Exists

This project exists to demonstrate:

- Event-loop driven networking
- Server-authoritative game logic
- Cross-language binary protocol
- Clean separation of networking and game logic

It is **educational**, not commercial.

---

## Roadmap / Optional Improvements

While the current implementation is fully functional, possible enhancements include:

- More robust input validation
- Optional GUI client
- Extended protocol messages (e.g., chat, lobby system)
- Security hardening and authentication
- Logging and debugging utilities
