#include "httplib.h"
#include <mysql/mysql.h>
#include <shared_mutex>
#include <iostream>
#include <unordered_map>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <csignal>
#define MAX_DB_CONNS 50
using namespace std;


struct kvnode{
    struct kvnode *prev;
    string key;
    string value;
    struct kvnode *next;
};

httplib::Server svr;

class Cache{ 
    private:
    unordered_map<string, struct kvnode*> cache_map;
    struct kvnode *head, *tail;
    int max_cache_size, curr_cache_size;
    public:
    Cache(){
        head=nullptr;
        tail=nullptr;
        max_cache_size = 3;
        curr_cache_size = 0;
    }

    void moveToFront(struct kvnode* newnode){
        newnode->prev->next = newnode->next;
        if(tail!=newnode)
            newnode->next->prev = newnode->prev;
        else
            tail = newnode->prev;
        newnode->prev = nullptr;
        head->prev = newnode;
        newnode->next = head;
        head = newnode;
    }
    public:
    bool add(string key, string value){
        if(cache_map.find(key)!=cache_map.end()){
            return false;
        }
        else{
            struct kvnode *newnode;
            newnode = new struct kvnode;
            newnode->key = key;
            newnode->value = value;
            newnode->prev = nullptr;
            if(curr_cache_size==max_cache_size){
                remove(tail->key);
            }
            if(head==nullptr && tail==nullptr){
                newnode->next = nullptr;
                tail = newnode;
            }
            else{
                newnode->next = head;
                head->prev = newnode;
            }
            head = newnode;
            cache_map[key] = newnode;
            curr_cache_size++;
            return true;
        }
    }

    bool read(string key, string *value){
        if (cache_map.find(key) != cache_map.end()) {
            struct kvnode *newnode = cache_map[key];
            if(newnode!=head)
                moveToFront(newnode);
            *value = newnode->value;
            return true;
        }
        else{
            return false;
        }
    }
    
    bool update(string key, string value){
        if (cache_map.find(key) != cache_map.end()) {
            struct kvnode *newnode = cache_map[key];
            if(newnode!=head)
                moveToFront(newnode);
            newnode->value = value;
            return true;
        }
        else{
            return false;
        }
    }

    bool remove(string key){
        if (cache_map.find(key) != cache_map.end()) {
            struct kvnode *newnode = cache_map[key];
            if(newnode!=head)
                newnode->prev->next = newnode->next;
            else{
                head = newnode->next;
                if(head!=nullptr)
                    head->prev = nullptr;
            }
            if(newnode!=tail)
                newnode->next->prev = newnode->prev;
            else{
                tail = newnode->prev;
                if(tail!=nullptr)
                    tail->next = nullptr;
            }
            delete newnode;
            cache_map.erase(key);
            curr_cache_size--;
            return true;
        }
        else
            return false;
    }
};
Cache cache_obj;
shared_mutex cache_rwlock;
condition_variable cv;
mutex mutx;
queue<MYSQL*> Queue;

MYSQL* get_connection(){
    {
        unique_lock<mutex> lock(mutx);
        while(Queue.empty()){
            cv.wait(lock);
        }
        MYSQL* conn =  Queue.front();
        Queue.pop();
        return conn;
    }
}

void put_connection(MYSQL* conn){
    {
        unique_lock<mutex> lock(mutx);
        Queue.push(conn);
        cv.notify_all();
    }
}

MYSQL* connect_db() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        cerr << "mysql_init() failed\n";
        exit(1);
    }

    if (!mysql_real_connect(conn, "localhost", "kv_user", "123456", "kvstore", 3306, nullptr, 0)) {
        cerr << "mysql_real_connect() failed: " << mysql_error(conn) << "\n";
        mysql_close(conn);
        exit(1);
    }

    return conn;
}

void signal_handler(int signal){
    while(!Queue.empty()){
        MYSQL* conn = Queue.front();
        mysql_close(conn);
        Queue.pop();
    }
    svr.stop();
    exit(0);
}

void create_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value = req.get_param_value("value");
    string query;
    {
        unique_lock<shared_mutex> lock(cache_rwlock);
        bool added = cache_obj.add(key,value);
        if(!added){
            res.set_content("Key Already Exists", "text/plain");
            return;
        }
    }
    query = "INSERT INTO kvpairs VALUES ('" + key + "', '" + value + "')";
    MYSQL* conn = get_connection();
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("Key Already Exists! Not created", "text/plain");
    }else{
        res.set_content("OK", "text/plain");
    }
    put_connection(conn);
}

void read_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value;

    {
        shared_lock<shared_mutex> lock(cache_rwlock);
        bool present = cache_obj.read(key, &value);
        if(present){
            res.set_content(value, "text/plain");
            return;
        }
    }

    MYSQL* conn = get_connection();
    string query = "SELECT value FROM kvpairs WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        value = row[0];
        {
            unique_lock<shared_mutex> lock(cache_rwlock);
            bool result = cache_obj.add(key,value);
        }
        res.set_content(value, "text/plain");
    } else {
        res.status = 404;
        res.set_content("Key not found", "text/plain");
    }
    mysql_free_result(result);
    put_connection(conn);
}

void update_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value = req.get_param_value("value");
    string query;
    bool present;
    {
        unique_lock<shared_mutex> lock(cache_rwlock);
        present = cache_obj.update(key,value);
    }
    MYSQL* conn = get_connection();
    query = "UPDATE kvpairs SET value='" + value + "' WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("Key not found", "text/plain");
    }else{
        res.set_content("OK", "text/plain");
        if(!present){
            {
                unique_lock<shared_mutex> lock(cache_rwlock);
                bool result = cache_obj.add(key,value);
            }
        }
    }
    put_connection(conn);
}

void delete_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    {
        unique_lock<shared_mutex> lock(cache_rwlock);
        bool result = cache_obj.remove(key);
    }

    MYSQL* conn = get_connection();
    string query = "DELETE FROM kvpairs WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    if(mysql_affected_rows(conn)==0){
        res.status = 404;
        res.set_content("Key not found", "text/plain");
    }
    else{
         res.set_content("OK", "text/plain");
    }
    put_connection(conn);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    MYSQL* conn = connect_db();
    string query = "CREATE TABLE IF NOT EXISTS kvpairs ( `key` VARCHAR(20) PRIMARY KEY, value TEXT NOT NULL)";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    mysql_close(conn);
    for(int i=0;i<MAX_DB_CONNS;i++){
        Queue.push(connect_db());
    }
    
    svr.Post("/create", create_handler);
    svr.Get("/read", read_handler);
    svr.Put("/update",update_handler);
    svr.Delete("/delete", delete_handler);
    cout << "Server running on port 8080...\n";
    svr.listen("0.0.0.0", 8080);
}