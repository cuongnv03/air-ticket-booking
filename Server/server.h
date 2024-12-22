#ifndef SERVER_H
#define SERVER_H

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
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <map>
#include <mutex>
#include <chrono>
#include <queue>
#include <algorithm>

using namespace std;

// Constants
#define PORT 3000
#define BUFFER_SIZE 1024

// Mutex for thread safety
std::map<int, string> clientNotifMap; // Map to store notifications for clients
std::mutex clientNotifMapMutex; // Mutex for notifications
std::map<std::string, int> userSocketMap; // Map to store user ID and client socket
std::mutex mapMutex; // Mutex for shared resources

// Structs to represent Flight, User, and Ticket data
struct Flight {
    string company;
    string flightId;
    int numSeatsA;
    int numSeatsB;
    int priceA;
    int priceB;
    string departurePoint;
    string destinationPoint;
    string departureDate;
    string returnDate;
};

struct User {
    int userId;
    string username;
    string password;
};

struct Ticket {
    string ticketId;
    int userId;
    string flightId;
    string seatClass;
    double ticketPrice;
    string paymentStatus;
};

struct DateDifference {
    int days;
    int hours;
};

// Utility functions
DateDifference calculateDateDifference(const string& oldDate, const string& newDate){
    // Assuming date format "YYYY-MM-DD"
    std::tm tm_old = {}, tm_new = {};
    std::istringstream ss_old(oldDate);
    std::istringstream ss_new(newDate);

    ss_old >> std::get_time(&tm_old, "%Y-%m-%d %H:%M");
    ss_new >> std::get_time(&tm_new, "%Y-%m-%d %H:%M");

    auto oldTime = std::chrono::system_clock::from_time_t(std::mktime(&tm_old));
    auto newTime = std::chrono::system_clock::from_time_t(std::mktime(&tm_new));

    auto duration = std::chrono::duration_cast<std::chrono::minutes>(newTime - oldTime);
    int days = duration.count() / (60 * 24);
    int hours = (duration.count() % (60 * 24)) / 60;

    return DateDifference{days, hours};
}
vector<string> splitString(const string& input, char delimiter){
    vector<string> result;
    stringstream ss(input);
    string token;
    while (getline(ss, token, delimiter)) {
        result.push_back(token);
    }
    return result;
}
string toLower(const string& input){
    string result = input;
    for (char &c : result){
        c = tolower(c);
    }
    return result;
}
string generateTicketId(){
    srand(time(NULL));

    const string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const int alphabet_length = alphabet.length();

    string ticketId;

    for (int i = 0; i < 3; ++i)
    {
        ticketId += alphabet[rand() % alphabet_length];
    }
    for (int i = 0; i < 3; ++i)
    {
        ticketId += to_string(rand() % 10);
    }
    return ticketId;
}
string checkNotifications(int clientSocket){
    string notification;
    {
        std::lock_guard<std::mutex> lock(clientNotifMapMutex);
        auto it = clientNotifMap.find(clientSocket);
        if (it != clientNotifMap.end())
        {
            notification = it->second;
            clientNotifMap.erase(it);
        }
    }
    return notification;
}

// User Authentication Functions
void logIn(int clientSocket, const string& username, const string& password);
void registerUser(int clientSocket, const string& username, const string& password);

// Flight Search Functions
void searchFlight1(int clientSocket, const string& departurePoint, const string& destinationPoint, const User& user);
void searchFlight2(int clientSocket, const string& departurePoint, const string& destinationPoint, const string& departureDate, const User& user);
void searchFlight3(int clientSocket, const string& departurePoint, const string& destinationPoint, const string& returnDate, const User& user);
void searchFlight4(int clientSocket, const string& departurePoint, const string& destinationPoint, const string& departureDate, const string& returnDate, const User& user);
void compareFlight(int clientSocket, const string& departurePoint, const string& destinationPoint, const string& departureDate, const string& seatClass, const string& order, const User& user);

// Flight Booking and Seat Management Functions
void bookFlight(int clientSocket, const string& flightId, const string& seatClass, const User& user);
void updateSeatCount(sqlite3* db, const string& flightId, const string& seatClass, int adjustment);

// Client Connection and User Interaction Functions
void connectClient(int clientSocket);
void handleUserFunctions(int clientSocket, const User& user);

// Payment and Ticket Management Functions
void processPayment(int clientSocket, const string& ticketId, const string &paymentMethod, const string &paymentDetails, const User& user);
void processRefund(int clientSocket, const string& ticketId, const string &paymentMethod, const string &paymentDetails, const User& user);
void processPaymentForChange(int clientSocket, const string& ticketId, const string &paymentMethod, const string &paymentDetails, const User& user);
void processRefundForChange(int clientSocket, const string& ticketId, const string &paymentMethod, const string &paymentDetails, const User& user);
void cancelTicket(int clientSocket, const string& ticketId, const User& user);
void changeTicket(int clientSocket, const string& ticketId, const string& newFlightId, const string& newSeatClass, const User& user);
// void printTicket(int clientSocket, const string& ticketId, const User& user);
// void viewTickets(int clientSocket, const User& user);
// bool flightIdExists(const string& flightId);

// Admin Functions
// void updateFlight1(int clientSocket, const string &flightId, const string &newDepartureDate);
// void updateFlight2(int clientSocket, string &flightId, const string &newReturnDate);
// void updateFlight3(int clientSocket, string &flightId, const string &newDepartureDate, const string &newReturnDate);

// Notification Functions
// void sendNotifications(int clientSocket, const vector<int>& affectedIds, const string& notification, int c);
// void notifyUsers(const vector<int>& affectedUserIds, const string& notification, int c);
// pair<string, string> getOldDates(const string &flightId);
// std::vector<int> getAffectedUserId(const std::string &flightId);
// std::string getUsernameFromId(int userId);

#endif // SERVER_H
