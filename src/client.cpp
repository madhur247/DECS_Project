#include "../include/httplib.h"
#include <iostream>
using namespace std;
int main() {

    while (true) {
        httplib::Client cli("192.168.82.1", 8080);
        string command, user_id, term;
        cout << "\nEnter command (create/read/readall/exit): ";
        cin >> command;

        if (command == "exit") break;

        cout << "Enter user_id: ";
        cin >> user_id;
        cin.ignore();

        if (command == "create") {
            cout << "Enter search term: ";
            getline(cin,term);
            string body = "user_id=" + user_id + "&term=" + term;
            auto res = cli.Post("/create", body, "application/x-www-form-urlencoded");
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else if (command == "read") {
            string count;
            cout<<"Enter a count < 5 (OR Press Enter for count=5): ";
            getline(cin,count);
            string query = "/read?user_id=" + user_id + "&count=" + count;
            auto res = cli.Get(query.c_str());
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } 
        else if (command == "readall") {
            string query = "/readall?user_id=" + user_id;
            auto res = cli.Get(query.c_str());
            if (res) cout << res->body << "\n";
            else cerr << "Request failed\n";
        } else {
            cout << "Unknown command\n";
        }
    }

    return 0;
}
