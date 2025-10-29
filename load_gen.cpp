#include "httplib.h"
#include <iostream>
#include <thread>
#include <random>
using namespace std;

void thread_handler(){
    random_device rd;
    mt19937 gen(rd());
    httplib::Client cli("localhost", 8080);

    while (true) {
        string command, key, value;
        uniform_int_distribution<> dist(1, 4);
        int cmd = dist(gen);
        uniform_int_distribution<> dist(1, 200);
        int key_int  = dist(gen);
        string key = to_string(key_int);

        if (command == "create") {
            cout << "Enter value: ";
            cin >> value;
            string body = "key=" + key + "&value=" + value;
            auto res = cli.Post("/create", body, "application/x-www-form-urlencoded");
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if (command == "read") {
            string query = "/read?key=" + key;
            auto res = cli.Get(query.c_str());
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if (command == "delete") {
            string query = "/delete?key=" + key;
            auto res = cli.Delete(query.c_str());
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else {
            cout << "Unknown command\n";
        }
    }
}

int main(int argc, char* argv[]) {
    if(argc<3){
        cout<<"Invalid arguments";
        return 1;
    }
    int threads_num = atoi(argv[1]);
    int time = atoi(argv[2]);
    if(threads_num == 0 || time == 0){
        cout<<"Invalid arguments";
        return 1;
    }
    thread threads[threads_num];
    for(int i=0;i<threads_num;i++){
        threads[i] = thread(thread_handler);
    }
    for(int i=0;i<threads_num;i++){
        threads[i].join(); 
    }

    return 0;
}
