#!/bin/bash

echo "Compiling files..."
g++ server.cpp -o server_multi -std=c++17 -lmysqlclient
g++ client.cpp -o client_multi -std=c++17
g++ reset_db.cpp -o reset_db -lmysqlclient
echo "Done"

# echo "Resetting Database..."
# ./reset_db

echo "Starting server..."
./server_multi &> server_multi.log &
KV_PID=$!
sleep 2

echo "Starting client..."
./client_multi 

echo "Stopping server..."
kill -SIGINT $KV_PID