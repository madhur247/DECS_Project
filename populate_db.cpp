#include <mysql/mysql.h>
#include <iostream>
#include <random>

using namespace std;

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


int main(int argc, char* argv[]){
    if(argc<2){
        cout<<"Invalid arguments"<<endl;
        return 1;
    }
    int rows = atoi(argv[1]);
    if(rows<=0){
        cout<<"Invalid arguments"<<endl;
        return 1;
    }
    MYSQL* conn = connect_db();
    string query = "CREATE TABLE IF NOT EXISTS history ( row_id INT AUTO_INCREMENT PRIMARY KEY, user_id VARCHAR(255) NOT NULL, term TEXT NOT NULL)";
    if(mysql_query(conn, query.c_str())!=0){
        cerr << "Query failed: " << mysql_error(conn) << "\n";
    }
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist3(1, 100);
    string user_id, term;
    for(int i=0;i<rows;i++){
        int term_len = dist3(gen);
        user_id = to_string(i%1000);
        term = generateRandomString(term_len);
        query = "INSERT INTO history (user_id, term) VALUES ('" + user_id + "', '" + term + "')";
        if(mysql_query(conn, query.c_str())!=0){
            cerr << "Query failed: " << mysql_error(conn) << "\n";
        }
    }
    mysql_close(conn);
}