#!/bin/bash

KV_SERVER="./server"          
LOAD_GEN="./loadgen"      
MYSQLD="/usr/sbin/mysqld"        

echo "Starting MySQL on CPU 0..."
taskset 0x100 $MYSQLD --defaults-file=/etc/mysql/my.cnf &> mysql.log &
MYSQL_PID=$!
sleep 5

echo "Starting KV server on CPU 1..."
taskset 0xF $KV_SERVER &> kv.log &
KV_PID=$!
sleep 2

echo "Starting load generator on CPU 2..."
taskset 0xF0 $LOAD_GEN $1 $2
LOAD_EXIT=$?

echo "Stopping KV server and MySQL..."
kill $KV_PID
kill $MYSQL_PID

exit $LOAD_EXIT
