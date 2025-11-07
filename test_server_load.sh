#!/bin/bash

echo "Compiling files..."
g++ server.cpp -o server -std=c++17 -lmysqlclient
g++ load_gen.cpp -o loadgen -std=c++17
g++ reset_db.cpp -o reset_db -lmysqlclient
g++ populate_db.cpp -o populate_db -lmysqlclient

echo "Resetting Database..."
./reset_db

if [ "$3" = "2" ]; then
echo "Populating Database"
./populate_db 1000000
elif [ "$3" = "3" ]; then
echo "Populating Database"
./populate_db 6000
fi

KV_SERVER="./server"          
LOAD_GEN="./loadgen"      
MYSQLD="/usr/sbin/mysqld"        

echo "Starting Mysql..."
MYSQL_PID=$(pidof mysqld)
taskset -cp 0,1 $MYSQL_PID &> mysql.log &
# MYSQL_PID=$!
sleep 5

echo "Starting KV server..."
taskset -c 2,3 $KV_SERVER &> kv.log &
KV_PID=$!
sleep 2

echo "Starting load generator..."
taskset -c 4,5,8,9,10 $LOAD_GEN $1 $2 $3
LOAD_EXIT=$?

echo "Stopping KV server and MySQL..."
kill -SIGINT $KV_PID
# kill -SIGINT $MYSQL_PID
#watch -n 1 "clear && dstat -cdm --disk-util 1 1"