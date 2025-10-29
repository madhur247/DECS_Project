#include "httplib.h"
#include <iostream>
using namespace std;
int main() {
    httplib::Client cli("localhost", 8080);

    while (true) {
        string command, key, value;
        cout << "\nEnter command (create/read/delete/exit): ";
        cin >> command;

        if (command == "exit") break;

        cout << "Enter key: ";
        cin >> key;

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

    return 0;
}
