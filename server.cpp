#include "httplib.h"
#include <mysql/mysql.h>
#include <iostream>
#include <unordered_map>
#include <mutex>
using namespace std;

struct kvnode{
    struct kvnode *prev;
    string key;
    string value;
    struct kvnode *next;
};

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
mutex cache_mutex;

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

void create_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value = req.get_param_value("value");
    string query;
    {
        lock_guard<mutex> lock(cache_mutex);
        bool added = cache_obj.add(key,value);
        if(!added){
            res.set_content("Key Already Exists! Not created", "text/plain");
            return;
        }
    }
    query = "INSERT INTO kvpairs VALUES ('" + key + "', '" + value + "')";
    MYSQL* conn = connect_db();
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("Key Already Exists! Not created", "text/plain");
    }else{
        res.set_content("Created", "text/plain");
    }
    mysql_close(conn);
}

void read_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value;

    {
        lock_guard<mutex> lock(cache_mutex);
        bool present = cache_obj.read(key, &value);
        if(present){
            res.set_content("Cache hit: " + value, "text/plain");
            return;
        }
    }

    MYSQL* conn = connect_db();
    string query = "SELECT value FROM kvpairs WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
       cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    MYSQL_RES* result = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        value = row[0];
        {
            lock_guard<mutex> lock(cache_mutex);
            bool result = cache_obj.add(key,value);
        }
        res.set_content("DB hit: " + value, "text/plain");
    } else {
        res.status = 404;
        res.set_content("Key not found", "text/plain");
    }
    mysql_free_result(result);
    mysql_close(conn);
}

void update_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value = req.get_param_value("value");
    string query;
    bool present;
    {
        lock_guard<mutex> lock(cache_mutex);
        present = cache_obj.update(key,value);
    }
    MYSQL* conn = connect_db();
    query = "UPDATE kvpairs SET value='" + value + "' WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("Key not found", "text/plain");
    }else{
        res.set_content("Updated", "text/plain");
        {
            if(!present){
                lock_guard<mutex> lock(cache_mutex);
                bool result = cache_obj.add(key,value);
            }
        }
    }
    mysql_close(conn);
    
}

void delete_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    {
        lock_guard<mutex> lock(cache_mutex);
        bool result = cache_obj.remove(key);
    }

    MYSQL* conn = connect_db();
    string query = "DELETE FROM kvpairs WHERE `key`='" + key + "'";
    if(mysql_query(conn, query.c_str())!=0){
       cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    if(mysql_affected_rows(conn)==0){
        res.status = 404;
        res.set_content("Key not found", "text/plain");
    }
    else{
         res.set_content("Deleted", "text/plain");
    }
    mysql_close(conn);
}

int main() {
    MYSQL* conn = connect_db();
    string query = "CREATE TABLE IF NOT EXISTS kvpairs ( `key` VARCHAR(20) PRIMARY KEY, value TEXT NOT NULL)";
    if(mysql_query(conn, query.c_str())!=0){
       cerr << "Query failed: " << mysql_error(conn) << "\n";
   }
    mysql_close(conn);
    httplib::Server svr;
    svr.Post("/create", create_handler);
    svr.Get("/read", read_handler);
    svr.Put("/update",update_handler);
    svr.Delete("/delete", delete_handler);
    cout << "Server running on port 8080...\n";
    svr.listen("0.0.0.0", 8080);
}
