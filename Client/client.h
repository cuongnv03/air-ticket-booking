#include <iostream>
#include <cstring>
#include <string>
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

// void save_all_tickets_to_file(const string &ticket_data)
// {
//     ofstream file("Ticket/All_tickets.txt");

//     if (!file.is_open())
//     {
//         cerr << "Failed to open file for writing." << endl;
//         return;
//     }

//     size_t pos = 0;
//     while (true)
//     {
//         size_t next_pos = ticket_data.find(';', pos);
//         if (next_pos == string::npos)
//         {
//             break;
//         }
//         string ticket_info = ticket_data.substr(pos, next_pos - pos);

//         size_t start = 0, end;
//         file << "---------------------" << endl;
//         const char *titles[] = {"Flight Number: ", "Ticket Code: ", "Company:", "Departure Point: ", "Destination Point: ", "Departure Date: ", "Return Date: ", "Seat Class: ", "Ticket Price: ", "Payment: "};
//         int field_index = 0;
//         while ((end = ticket_info.find(',', start)) != string::npos)
//         {
//             string field = ticket_info.substr(start, end - start);
//             file << titles[field_index++] << field << endl;
//             start = end + 1;
//         }

//         if (field_index < 10)
//         { // Check to avoid out of bounds access in titles
//             string lastField = ticket_info.substr(start);
//             file << titles[field_index] << lastField << endl;
//         }

//         file << "---------------------" << endl;

//         pos = next_pos + 1;
//     }

//     file.close();
//     cout << "Ticket information saved to Ticket/All_tickets.txt" << endl;
// }

// void save_tickets_to_file(const string &ticket_data, string ticket_code)
// {
//     string filename = ticket_code + ".txt";
//     string file_folder = "Ticket/" + filename;
//     ofstream file(file_folder);

//     if (!file.is_open())
//     {
//         cerr << "Failed to open file for writing." << endl;
//         return;
//     }

//     file << "---------------------" << endl;
//     const char *titles[] = {"Flight Number: ", "Ticket Code: ", "Company:", "Departure Point: ", "Destination Point: ", "Departure Date: ", "Return Date: ", "Seat Class: ", "Ticket Price: ", "Paymemt: "};
//     size_t start = 0, end;
//     int field_index = 0;
//     while ((end = ticket_data.find(',', start)) != string::npos)
//     {
//         string field = ticket_data.substr(start, end - start);
//         file << titles[field_index++] << field << endl; // Writes each field with a title
//         start = end + 1;
//     }
//     if (field_index < 10)
//     {
//         file << titles[field_index] << ticket_data.substr(start) << endl;
//     }

//     file << "---------------------" << endl;

//     file.close();
//     cout << "Ticket information saved to " << file_folder << endl;
// }

// void display_ticket_information(const string &ticket_data)
// {
//     size_t pos = 0;
//     while (true)
//     {
//         size_t next_pos = ticket_data.find(';', pos);
//         if (next_pos == string::npos)
//         {
//             break;
//         }
//         string ticket_info = ticket_data.substr(pos, next_pos - pos);

//         size_t start = 0, end;
//         cout << "---------------------" << endl;
//         const char *titles[] = {"Flight Number: ", "Ticket Code: ", "Company: ", "Departure Point: ", "Destination Point: ", "Departure Date: ", "Return Date: ", "Seat Class: ", "Ticket Price: ", "Payment: "};
//         int field_index = 0;

//         while (true)
//         {
//             end = ticket_info.find(',', start);
//             if (end == string::npos)
//             {
//                 cout << titles[field_index] << ticket_info.substr(start) << endl;
//                 break;
//             }
//             string field = ticket_info.substr(start, end - start);
//             cout << titles[field_index++] << field << endl;
//             start = end + 1;
//         }
//         cout << "---------------------" << endl;

//         pos = next_pos + 1;
//     }
// }
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
    std::cout << "Your choice(1-6): ";
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
