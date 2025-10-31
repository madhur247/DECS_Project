#!/bin/bash

KV_SERVER="./server"          
LOAD_GEN="./loadgen"      
MYSQLD="/usr/sbin/mysqld"        

echo "Starting MySQL on CPU 0..."
taskset -c 8 $MYSQLD --defaults-file=/etc/mysql/my.cnf &> mysql.log &
MYSQL_PID=$!
sleep 5

echo "Starting KV server on CPU 1..."
taskset -c 0,1,2,3,4,5 $KV_SERVER &> kv.log &
KV_PID=$!
sleep 2

echo "Starting load generator on CPU 2..."
taskset -c 6,7,9,10 $LOAD_GEN $1 $2
LOAD_EXIT=$?

echo "Stopping KV server and MySQL..."
kill $KV_PID
# kill $MYSQL_PID

exit $LOAD_EXIT
