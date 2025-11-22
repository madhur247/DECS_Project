#include "../include/httplib.h"
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

void thread_handler(int seconds, u_int64_t *thread_requests, double *thread_resptime, int load_type){
    random_device rd;
    int milli = seconds*1000;
    mt19937 gen(rd());
    httplib::Client cli("192.168.82.1", 8080);
    auto start = chrono::steady_clock::now();
    auto now = start;
    auto duration = chrono::duration_cast<chrono::milliseconds>(now-start);
    u_int64_t count = 0;
    auto req_sent = chrono::steady_clock::now();
    auto resp_recvd = req_sent;
    auto resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
    double total_resptime = 0;
    if(load_type == 1){
        do {
            string user_id;
            user_id = to_string(count%1000);
            uniform_int_distribution<> dist3(1, 100);
            int term_len = dist3(gen);
            string term = generateRandomString(term_len);
            string body = "user_id=" + user_id + "&term=" + term;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Post("/create", body, "application/x-www-form-urlencoded");
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
            // else cout<<res->body<<endl;
            count++;
            total_resptime+=resp_time.count();
            now = chrono::steady_clock::now();
            duration = chrono::duration_cast<chrono::milliseconds>(now-start);
        } while(duration.count()<milli);
    }
    else if(load_type == 2){
        string user_id;
        do {   
            user_id = to_string(count%100000);
            string query = "/read?user_id=" + user_id;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Get(query.c_str());
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
            // else cout<<res->body<<endl;
            count++;
            total_resptime+=resp_time.count();
            now = chrono::steady_clock::now();
            duration = chrono::duration_cast<chrono::milliseconds>(now-start);
        } while(duration.count()<milli);
    }
    else if(load_type == 3){
        string user_id;
        do {
            user_id = to_string(count%50);
            string rcount = to_string((count%5) + 1);
            string query = "/read?user_id=" + user_id + "&count=" + rcount;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Get(query.c_str());
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
            // else cout<<res->body<<endl;
            count++;
            total_resptime+=resp_time.count();
            now = chrono::steady_clock::now();
            duration = chrono::duration_cast<chrono::milliseconds>(now-start);
            
        } while(duration.count()<milli);
    }
    else if(load_type == 4){
       string user_id;
        do {   
            user_id = to_string(count%1000);
            string query = "/readall?user_id=" + user_id;
            req_sent = chrono::steady_clock::now();
            auto res = cli.Get(query.c_str());
            resp_recvd = chrono::steady_clock::now();
            resp_time = chrono::duration_cast<chrono::microseconds>(resp_recvd-req_sent);
            if (!res) cerr << "Request failed\n";
            // else cout<<res->body<<endl;
            count++;
            total_resptime+=resp_time.count();
            now = chrono::steady_clock::now();
            duration = chrono::duration_cast<chrono::milliseconds>(now-start);
        } while(duration.count()<milli);
    }
    *thread_requests = count;
    *thread_resptime = total_resptime/1000;
    cli.stop();
}

int main(int argc, char* argv[]) {
    if(argc<4){
        cout<<"Invalid arguments"<<endl;
        return 1;
    }
    int threads_num = atoi(argv[1]);
    int mins = atoi(argv[2]);
    int load_type = atoi(argv[3]);
    if(threads_num<=0 || mins<=0 || load_type<=0 || load_type>4){
        cout<<"Invalid arguments"<<endl;
        return 1;
    }
    int seconds = mins*60;
    cout<<"Starting load test..."<<endl;
    thread threads[threads_num];
    u_int64_t requests[threads_num];
    double resptimes[threads_num];
    for(int i = 0;i<threads_num;i++){
        threads[i] = thread(thread_handler, seconds, &requests[i], &resptimes[i], load_type);
    }

    for(int i = 0;i<threads_num;i++){
        threads[i].join();
    }

    long long total_requests = 0;
    double total_resp_time = 0;
    double throughput = 0;
    for (int i=0;i< threads_num;i++){
        total_requests+=requests[i];
        total_resp_time+=resptimes[i];
        throughput+=(requests[i]/(resptimes[i]/1000));
    }
    cout<< "Avg throughput (resp time) for "<<threads_num<<" threads = "<<throughput<<"req/s"<<endl;
    cout<< "Avg throughput (total duration) for "<<threads_num<<" threads = "<<(total_requests/seconds)<<"req/s"<<endl;
    cout<< "Avg response time for "<<threads_num<<" threads = "<<(total_resp_time/total_requests)<<"ms"<<endl;
    return 0;
}

