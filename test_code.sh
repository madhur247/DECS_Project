#!/bin/bash

echo "Compiling files..."
g++ src/server.cpp -o src/server_multi -std=c++17 -lmysqlclient
g++ src/client.cpp -o src/client_multi -std=c++17
g++ src/reset_db.cpp -o src/reset_db -lmysqlclient
echo "Done"

# echo "Resetting Database..."
# ./src/reset_db

echo "Starting server..."
./src/server_multi &> logs/server_multi.log &
KV_PID=$!
sleep 2

echo "Starting client..."
./src/client_multi 

echo "Stopping server..."
kill -SIGINT $KV_PID