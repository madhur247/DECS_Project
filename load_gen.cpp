#include "httplib.h"
#include <iostream>
#include <thread>
#include <random>
using namespace std;

string generateRandomString(size_t length) {
    const string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    random_device rd;  // Non-deterministic random number generator
    mt19937 gen(rd()); // Mersenne Twister engine
    uniform_int_distribution<> dist(0, charset.size() - 1);

    string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    return result;
}

void thread_handler(){
    random_device rd;
    mt19937 gen(rd());
    httplib::Client cli("localhost", 8080);

    while (true) {
        string command, key;
        uniform_int_distribution<> dist(1, 5);
        int cmd = dist(gen);
        uniform_int_distribution<> dist(1, 1000);
        int key_int  = dist(gen);
        string key = to_string(key_int);
        uniform_int_distribution<> dist(1, 1000);
        int val_len = dist(gen);

        if (cmd == 1) {
            string value = generateRandomString(val_len);
            string body = "key=" + key + "&value=" + value;
            auto res = cli.Post("/create", body, "application/x-www-form-urlencoded");
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if (cmd == 2) {
            string query = "/read?key=" + key;
            auto res = cli.Get(query.c_str());
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if(cmd == 3){
            string value = generateRandomString(val_len);
            string body = "key=" + key + "&value=" + value;
            auto res = cli.Put("/update", body, "application/x-www-form-urlencoded");
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if (cmd == 4) {
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
