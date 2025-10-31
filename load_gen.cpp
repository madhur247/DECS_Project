#include "httplib.h"
#include <iostream>
#include <thread>
#include <random>
#include <chrono>
#include <ctime>
using namespace std;

string generateRandomString(size_t length) {
    const string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, charset.size() - 1);

    string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(gen)];
    }
    return result;
}

void thread_handler(int seconds, int *thread_requests, float *thread_resptime){
    random_device rd;
    int milli = seconds*1000;
    mt19937 gen(rd());
    httplib::Client cli("192.168.82.1", 8080);
    auto start = chrono::steady_clock::now();
    auto now = start;
    auto duration = chrono::duration_cast<chrono::milliseconds>(now-start);
    int count = 0;
    auto req_sent = chrono::steady_clock::now();
    auto resp_recvd = req_sent;
    auto resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
    double total_resptime = 0;
    do {
        string command, key;
        uniform_int_distribution<> dist(1, 4);
        int cmd = dist(gen);
        uniform_int_distribution<> dist2(1, 1000);
        int key_int  = dist2(gen);
        key = to_string(key_int);
        uniform_int_distribution<> dist3(1, 100);
        int val_len = dist3(gen);

        if (cmd == 1) {
            string value = generateRandomString(val_len);
            string body = "key=" + key + "&value=" + value;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Post("/create", body, "application/x-www-form-urlencoded");
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
        } else if (cmd == 2) {
            string query = "/read?key=" + key;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Get(query.c_str());
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
        } else if(cmd == 3){
            string value = generateRandomString(val_len);
            string body = "key=" + key + "&value=" + value;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Put("/update", body, "application/x-www-form-urlencoded");
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
        } else if (cmd == 4) {
            string query = "/delete?key=" + key;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Delete(query.c_str());
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
        } else {
            cout << "Unknown command\n";
        }
        count++;
        total_resptime+=resp_time.count();
        now = chrono::steady_clock::now();
        duration = chrono::duration_cast<chrono::milliseconds>(now-start);
    } while(duration.count()<milli);
    *thread_requests = count;
    *thread_resptime = total_resptime/1000;
}

int main(int argc, char* argv[]) {
    if(argc<3){
        cout<<"Invalid arguments";
        return 1;
    }
    int threads_num = atoi(argv[1]);
    int mins = atoi(argv[2]);
    int seconds = mins*60;
    if(threads_num == 0 || mins == 0){
        cout<<"Invalid arguments";
        return 1;
    }
    thread threads[threads_num];
    int requests[threads_num];
    float resptimes[threads_num];
    for(int i = 0;i<threads_num;i++){
        threads[i] = thread(thread_handler, seconds, &requests[i], &resptimes[i]);
    }

    for(int i = 0;i<threads_num;i++){
        threads[i].join();
    }

    long total_requests = 0;
    double total_resp_time = 0;
    for (int i=0;i< threads_num;i++){
        total_requests+=requests[i];
        total_resp_time+=resptimes[i];
    }
    cout<< "Avg throughput for "<<threads_num<<" threads = "<<(total_requests/total_resp_time/1000)<<"req/s"<<endl;
    cout<< "Avg response time for "<<threads_num<<" threads = "<<(total_resp_time/total_requests)<<"ms"<<endl;
    return 0;
}

