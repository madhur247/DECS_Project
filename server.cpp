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
    public: 
    unordered_map<string, struct kvnode*> cache_map;
    private:
    struct kvnode *head, *tail;
    int max_cache_size, curr_cache_size;
    public:
    Cache(){
        head=NULL;
        tail=NULL;
        max_cache_size = 100;
        curr_cache_size = 0;
    }

    void moveToFront(struct kvnode* newnode){
        newnode->prev->next = newnode->next;
        if(tail!=newnode)
            newnode->next->prev = newnode->prev;
        else
            tail = newnode->prev;
        newnode->prev = NULL;
        head->prev = newnode;
        newnode->next = head;
        head = newnode;
    }
    public:
    bool add(string key, string value){
        struct kvnode *newnode;
        if(cache_map.find(key)!=cache_map.end()){
            newnode = cache_map[key];
            newnode->value = value;
            if(head!=newnode){
                moveToFront(newnode);
            }
            return false;
        }
        else{
            newnode = new struct kvnode;
            newnode->key = key;
            newnode->value = value;
            newnode->prev = NULL;
            if(curr_cache_size==max_cache_size){
                remove(tail->key);
            }
            if(head==NULL && tail==NULL){
                newnode->next = NULL;
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
    
    bool remove(string key){
        if (cache_map.find(key) != cache_map.end()) {
            struct kvnode *newnode = cache_map[key];
            if(newnode!=head)
                newnode->prev->next = newnode->next;
            else{
                head = newnode->next;
                if(head!=NULL)
                    head->prev = NULL;
            }
            if(newnode!=tail)
                newnode->next->prev = newnode->prev;
            else{
                tail = newnode->prev;
                if(tail!=NULL)
                    tail->next = NULL;
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
    mysql_real_connect(conn, "localhost", "kv_user", "123456", "kvstore", 0, nullptr, 0);
    return conn;
}

void create_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value = req.get_param_value("value");
    string query;
    {
        lock_guard<mutex> lock(cache_mutex);
        bool result = cache_obj.add(key,value);
        if(!result){
            query = "UPDATE kvpairs SET value='" + value + "' WHERE key='" + key + "'";
        }
        else{
            query = "INSERT INTO kvpairs (k, v) VALUES ('" + key + "', '" + value + "')";
        }
    }

    MYSQL* conn = connect_db();
    mysql_query(conn, query.c_str());
    mysql_close(conn);

    res.set_content("Created", "text/plain");
}

void read_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    string value;

    {
        lock_guard<mutex> lock(cache_mutex);
        bool result = cache_obj.read(key, &value);
        if(result){
            res.set_content("Cache hit: " + value, "text/plain");
            return;
        }
    }

    MYSQL* conn = connect_db();
    string query = "SELECT v FROM kvpairs WHERE k='" + key + "'";
    mysql_query(conn, query.c_str());
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

void delete_handler(const httplib::Request& req, httplib::Response& res) {
    string key = req.get_param_value("key");
    {
        lock_guard<mutex> lock(cache_mutex);
        bool result = cache_obj.remove(key);
    }

    MYSQL* conn = connect_db();
    string query = "DELETE FROM kvpairs WHERE k='" + key + "'";
    mysql_query(conn, query.c_str());
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
    httplib::Server svr;
    svr.Post("/create", create_handler);
    svr.Get("/read", read_handler);
    svr.Delete("/delete", delete_handler);
    cout << "Server running on port 8080...\n";
    svr.listen("0.0.0.0", 8080);
}
