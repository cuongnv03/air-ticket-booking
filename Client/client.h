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
#include <chrono>

#define MAXLINE 4096
#define SERV_PORT 3000
#define BUFFER_SIZE 2048

std::mutex mapMutex;

using namespace std;

enum class Role {
    None,
    Admin,
    User
};

void displaySearchResults(const string &ticketData);
void printSearchMenu();
void printMainMenu();
void printUserFunctions();
void save_tickets_to_file(const string &ticket_data, string ticket_code);
string toLower(const string &input);
string trimString(const string &input);

// ==================== IMPLEMENTATIONS ====================

void displaySearchResults(const string &ticketData) {
    size_t pos = 0;
    while (true) {
        size_t nextPos = ticketData.find(';', pos);
        if (nextPos == string::npos) {
            break;
        }
        string ticketInfo = ticketData.substr(pos, nextPos - pos);

        const char *titles[] = {
            "Company: ", "Flight Number: ", "Seat Class A: ", "Seat Class B: ",
            "Price A: ", "Price B: ", "Departure Point: ", "Destination Point: ",
            "Departure Date: ", "Return Date: "
        };

        cout << "---------------------" << endl;
        size_t start = 0, end;
        int fieldIndex = 0;

        while (true) {
            end = ticketInfo.find(',', start);
            if (end == string::npos) {
                cout << titles[fieldIndex] << ticketInfo.substr(start) << endl;
                break;
            }
            cout << titles[fieldIndex++] << ticketInfo.substr(start, end - start) << endl;
            start = end + 1;
        }

        cout << "---------------------" << endl;
        pos = nextPos + 1;
    }
}

void printSearchMenu() {
    cout << "1. Search based on departure point and destination point.\n"
         << "2. Search based on departure point, destination point, and departure date.\n"
         << "3. Search based on departure point, destination point, and return date.\n"
         << "4. Search based on departure point, destination point, departure date, and return date.\n"
         << "5. Compare by sorting based on departure point, destination point, departure date and price \n"
         << "6. Exit.\n"
         << "Your choice (1-6): ";
}

void printMainMenu() {
    cout << "\n__________________________________________________\n"
         << "1. Login\n2. Register\n3. Exit\nYour choice: ";
}

void printUserFunctions() {
    cout << "\n__________________________________________________\n"
         << "1. Search Flights\n2. Book Tickets\n3. View Ticket Details\n4. Cancel Tickets\n"
         << "5. Change Tickets\n6. Print Tickets\n7. Ticket Payment\n8. Log Out\n"
         << "__________________________________________________\nYour choice: ";
}

string toLower(const string &input) {
    string result = input;
    for (char &c : result) {
        c = tolower(c);
    }
    return result;
}

string trimString(const string &input) {
    size_t endPos = input.find_last_not_of(" \t");
    if (endPos != string::npos) {
        return input.substr(0, endPos + 1);
    }
    return input;
}
void save_tickets_to_file(const string &ticket_data, string ticket_code)
{
    string filename = ticket_code + ".txt";
    string file_folder = "Ticket/" + filename;
    ofstream file(file_folder);

    if (!file.is_open())
    {
        cerr << "Failed to open file for writing." << endl;
        return;
    }

    file << "---------------------" << endl;
    const char *titles[] = {"Flight Number: ", "Ticket Code: ", "Company:", "Departure Point: ", "Destination Point: ", "Departure Date: ", "Return Date: ", "Seat Class: ", "Ticket Price: ", "Paymemt: "};
    size_t start = 0, end;
    int field_index = 0;
    while ((end = ticket_data.find(',', start)) != string::npos)
    {
        string field = ticket_data.substr(start, end - start);
        file << titles[field_index++] << field << endl; // Writes each field with a title
        start = end + 1;
    }
    if (field_index < 10)
    {
        file << titles[field_index] << ticket_data.substr(start) << endl;
    }

    file << "---------------------" << endl;

    file.close();
    cout << "Ticket information saved to " << file_folder << endl;
}
