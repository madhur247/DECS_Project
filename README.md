# HTTP-Based Key-Multivalue Server with LRU Caching

This project implements a key-multivalue search history service using a C++ HTTP server, an LRU cache in DRAM, and a MySQL database for persistence.

## Components
- HTTP Client: Send GET and POST requests.
- C++ HTTP Server: Built using `cpp-httplib`, listens on port 8080.
- LRU Cache: Stores up to five recent search terms per user using a hashmap and doubly linked list.
- MySQL Database: Stores persistent search history.

## Endpoints

### POST /create
- Inserts a search term for a given user.
- Adds the term to both the cache and the database.
- Creates a new key if not found.

### GET /read
- Retrieves up to five recent terms for a user.
- Returns from cache if available, otherwise queries the database.

### GET /readall
- Fetches all terms for a user directly from the database.
- Returns error if the key is not found.

## Architecture

![System Architecture Diagram](DECS_Project_architecture.png)

The server prioritizes cache hits for fast access and falls back to the database on misses. The LRU cache ensures efficient memory usage and fast lookups.

## Setup

1. Create a user `kv_user` with password `123456`.
2. Make sure you have a c++ compiler installed in your system.
3. Run the test_code.sh file

