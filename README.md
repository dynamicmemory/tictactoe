# Networked Tic-Tac-Toe (C)

This repository contains a **work-in-progress refactor** of a networked Tic-Tac-Toe server and client written in C.

The goal of this project is **not** to ship a polished game.  
The goal is to learn and practice **network server architecture**, specifically:

- Socket lifecycle management
- Event-loop–driven servers (`select`)
- Separation of concerns (transport vs logic)
- Incremental refactoring away from “everything in `main`”
- Preparing the codebase for future protocol hardening and security work

---

## Current State

This codebase is intentionally **mid-refactor**.

What has been done:
- Socket setup extracted into `server_listen`
- Accept logic extracted into `server_accept`
- Client handling extracted into `server_handle_client`
- Event loop isolated in `run_server`
- Client connection logic extracted from `main`

What is **not done yet** (by design):
- Proper protocol framing (currently string-based)
- Per-client state structs
- Server-authoritative turn enforcement
- Robust input validation
- Security features (authentication, rate limiting, etc.)
- Clean separation between networking and game logic

If you’re looking for “best practices”, this repo is **about learning why they exist**, not showcasing them.

---

## Build

### Server
```sh
gcc -Wall -Wextra -o server server.c 
```

### Client 
```sh
gcc -Wall -Wextra -o client client.c
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

Two clients are expected.

## Architecture (So Far)

High-level structure (still evolving):

### Transport
- `server_listen`
- `server_accept`

### Event Loop
- `run_server`

### Connection Handling
- `server_handle_client`

### Game Logic
- Currently global and intentionally marked for extraction

This project is being refactored toward a layered design where:

- The event loop does not know game rules
- The server is authoritative over game state
- Clients are treated as untrusted input sources

---

## Why This Exists

This project exists to break bad habits early:

- Stuffing all logic into `main`
- Treating sockets as players
- Conflating networking with application logic
- Hand-waving “security” instead of designing for it

Each refactor step is deliberate and educational.

---

## Roadmap

Planned next steps:

- Introduce per-client state structures
- Define a minimal binary protocol
- Move game state into an explicit game module
- Enforce server-side turn ownership
- Add basic hardening (validation, rate limits)
- Compare C vs C++ designs using the same architecture

---

## Disclaimer

This code is **not** production-ready.  
It is **not** secure.  
It is **not** idiomatic yet.


