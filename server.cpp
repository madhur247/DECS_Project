#include "httplib.h"
#include <mysql/mysql.h>
#include <mutex>
#include <iostream>
#include <unordered_map>
#include <condition_variable>
#include <queue>
#include <csignal>
#include <deque>
#include "json.hpp"
#define MAX_DB_CONNS 50

using namespace std;

struct kvnode{
    struct kvnode *prev;
    string user_id;
    deque<string> terms;
    int index;
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
        max_cache_size = 500;
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
    void add(string user_id, string term){
        if(cache_map.find(user_id)!=cache_map.end()){
            struct kvnode *newnode = cache_map[user_id];
            if(newnode->index < 4){
                newnode->terms.push_front(term);
                newnode->index++;
            }
            else{
                newnode->terms.pop_back();
                newnode->terms.push_front(term);
            }
        }
        else{
            struct kvnode *newnode;
            newnode = new struct kvnode;
            newnode->user_id = user_id;
            newnode->terms.push_front(term);
            newnode->index = 0;
            newnode->prev = nullptr;
            if(curr_cache_size==max_cache_size){
                remove_tail();
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
            cache_map[user_id] = newnode;
            curr_cache_size++;
        }
    }

    bool read(string user_id, deque<string> *value){
        if (cache_map.find(user_id) != cache_map.end()) {
            struct kvnode *newnode = cache_map[user_id];
            if(newnode!=head)
                moveToFront(newnode);
            *value = newnode->terms;
            return true;
        }
        else{
            return false;
        }
    }
    
    bool insert(string user_id, deque<string> terms){
        if (cache_map.find(user_id) == cache_map.end()) {
            struct kvnode *newnode;
            newnode = new struct kvnode;
            newnode->user_id = user_id;
            newnode->terms = terms;
            newnode->index = terms.size()-1;
            newnode->prev = nullptr;
            if(curr_cache_size==max_cache_size){
                remove_tail();
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
            cache_map[user_id] = newnode;
            curr_cache_size++;
            return true;
        }
        else{
            return false;
        }
    }

    bool remove_tail(){
        if (tail!=nullptr) {
            string user_id = tail->user_id;
            if(head!=tail){
                tail->prev->next = tail->next;
            }
            else{
                head = nullptr;
            }
            struct kvnode *ptr = tail;
            tail = tail->prev;
            ptr->terms.clear();
            delete ptr;
            cache_map.erase(user_id);
            curr_cache_size--;
            return true;
        }
        else
            return false;
    }
};
Cache cache_obj;
mutex cache_lock;
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
    string user_id = req.get_param_value("user_id");
    string term = req.get_param_value("term");
    string query;
    {
        unique_lock<mutex> lock(cache_lock);
        cache_obj.add(user_id,term);
    }
    query = "INSERT INTO history (user_id, term) VALUES ('" + user_id + "', '" + term + "')";
    MYSQL* conn = get_connection();
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("DB Error", "text/plain");
    }else{
        res.set_content("OK", "text/plain");
    }
    put_connection(conn);
}

void read_handler(const httplib::Request& req, httplib::Response& res) {
    string user_id = req.get_param_value("user_id");
    string count = req.get_param_value("count");
    if(count.empty()){
        count = "5";
    }
    int c = stoi(count);
    if(c>5)
    c=5;
    deque<string> terms, ret_terms;

    {
        unique_lock<mutex> lock(cache_lock);
        bool present = cache_obj.read(user_id, &terms);
        if(present){
            if(c>terms.size())
            c = terms.size();
            nlohmann::json json_body = deque<string>(terms.begin(),terms.begin()+c);
            res.set_content("Cache hit: "+json_body.dump(), "application/json");
            return;
        }
    }

    MYSQL* conn = get_connection();
    string query = "SELECT term FROM history WHERE user_id='" + user_id + "' ORDER BY row_id DESC LIMIT 5";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("DB Error","text/plain");
    }
    else{
        MYSQL_RES* result = mysql_store_result(conn);
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result))) {
            string term = row[0];
            terms.push_back(term);
            if(c-- > 0){
                ret_terms.push_back(term);
            }
        }
        if(terms.size()>0){
            {
                unique_lock<mutex> lock(cache_lock);
                cache_obj.insert(user_id,terms);
            }
            nlohmann::json json_body = ret_terms;
            res.set_content("DB hit: "+json_body.dump(), "application/json");
        }
        else {
            res.status = 404;
            res.set_content("Key not found", "text/plain");
        }
        mysql_free_result(result);
    }
    put_connection(conn);
}

void readall_handler(const httplib::Request& req, httplib::Response& res) {
    string user_id = req.get_param_value("user_id");
    deque<string> terms, terms5;
    MYSQL* conn = get_connection();
    string query = "SELECT term FROM history WHERE user_id='" + user_id + "' ORDER BY row_id DESC";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
        res.set_content("DB Error","text/plain");
    }
    else{
        MYSQL_RES* result = mysql_store_result(conn);
        MYSQL_ROW row;
        int count=0;
        while ((row = mysql_fetch_row(result))) {
            string term = row[0];
            terms.push_back(term);
            if(count<5){
                terms5.push_back(term);
                count++;
            }
        }
        if(terms.size()>0){
            {
                unique_lock<mutex> lock(cache_lock);
                cache_obj.insert(user_id,terms5);
            }
            nlohmann::json json_body = terms;
            res.set_content(json_body.dump(), "application/json");
        }
        else {
            res.status = 404;
            res.set_content("Key not found", "text/plain");
        }
        mysql_free_result(result);
    }
    put_connection(conn);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    MYSQL* conn = connect_db();
    string query = "CREATE TABLE IF NOT EXISTS history ( row_id INT AUTO_INCREMENT PRIMARY KEY, user_id VARCHAR(255) NOT NULL, term TEXT NOT NULL)";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    mysql_close(conn);
    for(int i=0;i<MAX_DB_CONNS;i++){
        Queue.push(connect_db());
    }
    
    svr.Post("/create", create_handler);
    svr.Get("/read", read_handler);
    svr.Get("/readall",readall_handler);
    cout << "Server running on port 8080...\n";
    svr.listen("0.0.0.0", 8080);
}