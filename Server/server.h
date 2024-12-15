#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <regex>
#include <unordered_map>
#include <thread>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sqlite3.h>
#include <cstring>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <mutex>
#include <chrono>
#include <queue>

using namespace std;
#define PORT 3000
#define BUFFER_SIZE 1024

std::map<int, string> clientNotifMap;
std::mutex clientNotifMapMutex;
std::map<std::string, int> userSocketMap; // client socket, user_id
std::mutex mapMutex;

struct Flights
{
    string company;
    string flight_num;
    int num_A;
    int num_B;
    int price_A;
    int price_B;
    string departure_point;
    string destination_point;
    string departure_date;
    string return_date;
};

struct User
{
    int user_id;
    string username;
    string password;
};

struct Ticket
{
    string ticket_code;
    int user_id;
    string flight_num;
    string seat_class;
    double ticket_price;
    string payment;
};

struct DateDifference
{
    int days;
    int hours;
};

DateDifference calculate_date_difference(const string &old_date, const string &new_date)
{
    std::tm tm_old = {}, tm_new = {};
    std::istringstream ss_old(old_date);
    std::istringstream ss_new(new_date);

    ss_old >> std::get_time(&tm_old, "%Y-%m-%d %H:%M");
    ss_new >> std::get_time(&tm_new, "%Y-%m-%d %H:%M");

    auto old_time = std::chrono::system_clock::from_time_t(std::mktime(&tm_old));
    auto new_time = std::chrono::system_clock::from_time_t(std::mktime(&tm_new));

    auto duration = std::chrono::duration_cast<std::chrono::minutes>(new_time - old_time);
    int days = duration.count() / (60 * 24);
    int hours = (duration.count() % (60 * 24)) / 60;

    return DateDifference{days, hours};
}

vector<string> split(const string &input, char delimiter)
{
    vector<string> result;
    stringstream ss(input);
    string item;

    while (getline(ss, item, delimiter))
    {
        result.push_back(item);
    }

    return result;
}
string lower(const string &input)
{
    string result = input;

    for (char &c : result)
    {
        c = tolower(c);
    }

    return result;
}
string generate_ticket_code()
{
    srand(time(NULL));

    const string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const int alphabet_length = alphabet.length();

    string ticket_code;

    for (int i = 0; i < 3; ++i)
    {
        ticket_code += alphabet[rand() % alphabet_length];
    }
    for (int i = 0; i < 3; ++i)
    {
        ticket_code += to_string(rand() % 10);
    }
    return ticket_code;
}
std::string checknoti(int client_socket)
{
    string notification;
    {
        std::lock_guard<std::mutex> lock(clientNotifMapMutex);
        auto it = clientNotifMap.find(client_socket);
        if (it != clientNotifMap.end())
        {
            notification = it->second;
            clientNotifMap.erase(it);
        }
    }
    return notification;
}
void log_in(int client_socket, const string &username, const string &password);
void register_user(int client_socket, const string &username, const string &password);
void search_flight1(int client_socket, const string &departure_point, const string &destination_point, const User &user);
// void search_flight2(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const User &user);
// void search_flight3(int client_socket, const string &company, const string &departure_point, const string &destination_point, const User &user);
// void search_flight4(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user);
// void search_flight5(int client_socket, const string &company, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user);
void functions(int client_socket, const User &user);
void connect_client(int client_socket);
// void admin_mode(int client_socket);
void book_flight(int client_socket, const string flight_num, const string seat_class, const User &user);
// void cancel_flight(int client_socket, const string ticket_code, const User &user);
void update_seat_count(sqlite3 *db, const string &flight_num, const string &seat_class, int adjustment);
// void handle_payment(int client_socket, const string ticket_code, string payment_status);
// void change_flight(int client_socket, const string ticket_code, const string flight_num_new, const string seat_class_new, const User &user);
// void print_all(int client_socket, const User &user);
// void print_ticket(int client_socket, const string ticket_code, const User &user);
// void handle_view(int client_socket, const User &user);
// bool flight_num_exists(const string &flight_num);
// void update_flight1(int client_socket, const string &flight_num, const string &new_departure_date);
// void update_flight2(int client_socket, string &flight_num, const string &new_return_date);
// void update_flight3(int client_socket, string &flight_num, const string &new_departure_date, const string &new_return_date);
// void notify_affected_users(const vector<int> &affected_user_ids, const string &noti, int c);
// pair<string, string> get_old_dates(const string &flight_num);
// std::vector<int> get_affected_user_id(const std::string &flight_num);
// std::string get_username_from_id(int user_id);
// pair<string, string> get_old_dates(const string &flight_num);
// void handle_notifications(int client_socket, vector<int> affected_ids, const string noti, int c);
