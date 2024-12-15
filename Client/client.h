#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <regex>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <atomic>
#include <thread>
#include <mutex>
using namespace std;
#include <chrono>

std::mutex mapMutex;

#define MAXLINE 4096
#define SERV_PORT 3000
#define BUFFER_SIZE 2048

void display_search(const string &ticket_data)
{
    size_t pos = 0;
    while (true)
    {
        size_t next_pos = ticket_data.find(';', pos);
        if (next_pos == string::npos)
        {
            break;
        }
        string ticket_info = ticket_data.substr(pos, next_pos - pos);

        size_t start = 0, end;
        cout << "---------------------" << endl;
        const char *titles[] = {"Company: ", "Flight Number: ", "Seat class A: ", "Seat class B: ", "Price A: ", "Price B: ", "Departure Point: ", "Destination Point: ", "Departure Date: ", "Return Date: "};
        int field_index = 0;

        while (true)
        {
            end = ticket_info.find(',', start);
            if (end == string::npos)
            {
                cout << titles[field_index] << ticket_info.substr(start) << endl;
                break;
            }
            string field = ticket_info.substr(start, end - start);
            cout << titles[field_index++] << field << endl;
            start = end + 1;
        }
        cout << "---------------------" << endl;

        pos = next_pos + 1;
    }
}

void print_menu_search()
{
    std::cout << "1. Search based on departure point, destination point\n";
    std::cout << "2. Search based on departure point, destination point, departure date\n";
    std::cout << "3. Search based on departure point, destination point, return date\n";
    std::cout << "4. Search based on departure point, destination point, departure date, return date\n";
    std::cout << "5. Exit\n";
    std::cout << "Your choice(1-5): ";
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
enum class Role
{
    none,
    admin,
    user
};
void print_functions()
{
    std::cout << "__________________________________________________\n";
    std::cout << "1. Search Flights\n2. Book tickets\n3. View tickets detail\n4. Cancel tickets\n5. Change tickets\n6. Print tickets\n7. Ticket payment\n8. Log out" << endl;
    std::cout << "__________________________________________________\n";
    std::cout << "Your message: ";
}
// void print_admin_menu()
// {
//     std::cout << "__________________________________________________\n";
//     std::cout << "1. Add flight\n2. Delete flight\n3. Modify flight\n4. Logout" << endl;
//     std::cout << "__________________________________________________\n";
//     std::cout << "Your message: ";
// }
void print_main_menu()
{
    std::cout << "__________________________________________________\n";
    std::cout << "1. Login\n2. Register\n3. Exit\nYour message: ";
}
std::string trim(std::string str)
{
    size_t endpos = str.find_last_not_of(" \t");

    if (std::string::npos != endpos)
    {
        str = str.substr(0, endpos + 1);
    }
    return str;
}
