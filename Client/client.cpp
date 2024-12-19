#include "client.h"

int main() {
    std::string tmpNotification;
    std::cout << "Enter server IP: ";

    std::string serverIP;
    getline(cin, serverIP);
    const char *host = serverIP.c_str();

    struct sockaddr_in serverAddr;
    int clientSocket;
    Role currentRole = Role::None;

    try {
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == -1) {
            throw runtime_error("Error creating client socket");
        }

        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(SERV_PORT);
        if (inet_pton(AF_INET, host, &serverAddr.sin_addr) <= 0) {
            cerr << "Invalid IP address." << endl;
            return 1;
        }

        if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
            throw runtime_error("Error connecting to the server.");
        }

        char buffer[BUFFER_SIZE];

        while (true) {
            printMainMenu();
            string choice;
            getline(cin, choice);
            string trimmedChoice = trimString(toLower(choice));

            if (trimmedChoice == "3") { // Exit
                send(clientSocket, trimmedChoice.c_str(), choice.length(), 0);
                break;
            } else if (trimmedChoice == "1") { // Login
                string username, password;
                cout << "Enter username: ";
                getline(cin, username);
                cout << "Enter password: ";
                getline(cin, password);

                string loginMessage = "login/" + username + "/" + password;
                send(clientSocket, loginMessage.c_str(), loginMessage.length(), 0);
            } else if (trimmedChoice == "2") { // Register
                string username, password;
                cout << "Enter new username: ";
                getline(cin, username);
                cout << "Enter new password: ";
                getline(cin, password);

                string registerMessage = "register/" + username + "/" + password;
                send(clientSocket, registerMessage.c_str(), registerMessage.length(), 0);
            } else {
                cout << "Invalid choice! Please try again." << endl;
                continue;
            }

            memset(buffer, 0, BUFFER_SIZE);
            int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (bytesReceived <= 0) {
                break;
            }

            buffer[bytesReceived] = '\0';
            string response(buffer);

            if (response == "210/" || response == "220/") { // Successfully logged in
                cout << "You are now logged in!" << endl;
                currentRole = Role::User;

                while (true) {
                    if (!tmpNotification.empty()) {
                        cout << "Notification:\n" << tmpNotification;
                        tmpNotification.clear();
                    }

                    printUserFunctions();
                    string userChoice;
                    getline(cin, userChoice);
                    string trimmedUserChoice = trimString(toLower(userChoice));

                    if (trimmedUserChoice == "8") { // Logout
                        send(clientSocket, "logout", strlen("logout"), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        int logoutResponse = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        if (logoutResponse > 0) {
                            buffer[logoutResponse] = '\0';
                            if (string(buffer) == "230/") {
                                currentRole = Role::None;
                                cout << "Logged out successfully." << endl;
                                break;
                            }
                        }
                        break;
                    } else if (trimmedUserChoice == "1") { // Search Flights
                        string searchRequest;
                        string company, destinationPoint, departurePoint, departureDate, returnDate, seatClass;
                        while (true){
                            printSearchMenu();
                            string searchChoice;
                            getline(cin, searchChoice);
                        
                            if (searchChoice == "1") { // Basic Search
                                cout << "Enter departure point: ";
                                getline(cin, departurePoint);
                                cout << "Enter destination point: ";
                                getline(cin, destinationPoint);
                                searchRequest = "search1/" + departurePoint + "/" + destinationPoint;
                                break;
                            } else if (searchChoice == "3"){
                                cout << "Enter departure point : ";
                                getline(cin, departurePoint);
                                cout << "Enter destination point : ";
                                getline(cin, destinationPoint);
                                cout << "Enter return date (YYYY-MM-DD): ";
                                getline(cin, returnDate);
                                searchRequest += "search3/"  + departurePoint + "/" + destinationPoint+ "/" + returnDate;
                                break;
                            } else if (searchChoice == "2") { // Search by Departure, Destination, and Date
                                cout << "Enter departure point: ";
                                getline(cin, departurePoint);
                                cout << "Enter destination point: ";
                                getline(cin, destinationPoint);
                                cout << "Enter departure date (YYYY-MM-DD): ";
                                getline(cin, departureDate);
                                searchRequest = "search2/" + departurePoint + "/" + destinationPoint + "/" + departureDate;
                                break;
                            } else if (searchChoice == "4") { // Search by Departure, Destination, and Return Date
                                cout << "Enter departure point: ";
                                getline(cin, departurePoint);
                                cout << "Enter destination point: ";
                                getline(cin, destinationPoint);
                                cout << "Enter departure date (YYYY-MM-DD): ";
                                getline(cin, departureDate);
                                cout << "Enter return date (YYYY-MM-DD): ";
                                getline(cin, returnDate);
                                searchRequest = "search4/" + departurePoint + "/" + destinationPoint + "/" + departureDate + "/" + returnDate;
                                break;
                            } else if (searchChoice == "5") {
                                cout << "Enter departure point: ";
                                getline(cin, departurePoint);
                                cout << "Enter destination point: ";
                                getline(cin, destinationPoint);
                                cout << "Enter departure date (YYYY-MM-DD): ";
                                getline(cin, departureDate);
                                cout << "Choose seat class (A or B): ";
                                getline(cin, seatClass);
                                string order;
                                while (true) {
                                    cout << "Choose order (ASC or DESC): ";
                                    getline(cin, order);
                                    if (order == "ASC" || order == "DESC") {
                                        break;
                                    } else {
                                        cout << "Invalid order! Please enter 'ASC' or 'DESC'." << endl;
                                    }
                                }
                                searchRequest += "compare/" + departurePoint + "/" + destinationPoint + "/" + departureDate + "/" + seatClass + "/" + order;
                                break;
                            } else if (searchChoice == "6") { // Exit Search
                                cout << "Exiting search..." << endl;
                                send(clientSocket, "exit search request", strlen("exit search request"), 0);
                                break;
                            } else {
                                cout << "Invalid choice! Please try again." << endl;
                            }
                        }

                        if (!searchRequest.empty()) {
                            send(clientSocket, searchRequest.c_str(), searchRequest.length(), 0);
                            memset(buffer, 0, BUFFER_SIZE);
                            int searchResponse = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                            if (searchResponse > 0) {
                                buffer[searchResponse] = '\0';
                                displaySearchResults(string(buffer));
                                if (string(buffer).find("311/") == 0) {
                                    string flightData = string(buffer).substr(8);
                                    cout << "Search results:" << endl;
                                    displaySearchResults(flightData);
                                } else if (string(buffer).find("312/") == 0) {
                                    string flightData = string(buffer).substr(8);
                                    cout << "Search results:" << endl;
                                    displaySearchResults(flightData);
                                } else if (string(buffer).find("313/") == 0) {
                                    string flightData = string(buffer).substr(8);
                                    cout << "Search results:" << endl;
                                    displaySearchResults(flightData);
                                } else if (string(buffer).find("314/") == 0) {
                                    string flightData = string(buffer).substr(8);
                                    cout << "Search results:" << endl;
                                    displaySearchResults(flightData);
                                } else if (string(buffer).find("411/") == 0) {
                                    cout << "No flights found." << endl;
                                } else if (string(buffer).find("421/") == 0) {
                                    cout << "No flights found for comparison." << endl;
                                }
                            }
                        }
                    } else if (trimmedUserChoice == "2") { // Book Flight
                        string flightId, seatClass;
                        cout << "Enter flight ID: ";
                        getline(cin, flightId);
                        cout << "Enter your desired seat class (A/B): ";
                        getline(cin, seatClass);

                        string bookRequest = "book/" + flightId + "/" + seatClass;
                        send(clientSocket, bookRequest.c_str(), bookRequest.length(), 0);

                        memset(buffer, 0, BUFFER_SIZE);
                        recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        string bookResponse(buffer);
                        istringstream bookResponseStream(bookResponse);
                        string bookResponseCode, ticketId, ticketPrice;
                        getline(bookResponseStream, bookResponseCode, '/');
                        if (bookResponseCode == "330"){
                            getline(bookResponseStream, ticketId, '/');
                            getline(bookResponseStream, ticketPrice, '/');
                            cout << "You are booking flight ticket with ticket ID: " << ticketId
                                << " and you need to pay " << ticketPrice << "đ to complete booking.\n";
                            // Automatically redirect to pay method
                            string payMethod, payDetails;
                            cout << "Enter payment method (Card/E-Wallet): ";
                            cin >> payMethod;
                            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear remaining input
                            if (payMethod != "Card" && payMethod != "E-Wallet"){
                                cout << "Invalid payment method. Please choose 'Card' or 'E-Wallet'." << endl;
                                continue;
                            }
                            cout << "Enter payment details: ";
                            cin >> payDetails;
                            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // Clear remaining input
                            if ((payMethod == "Card" && payDetails.length() != 16) || (payMethod == "E-Wallet" && payDetails.length() != 10)){
                                cout << "Invalid payment details format." << endl;
                                continue;
                            }
                            string payRequest = "pay/" + ticketId + "/" + ticketPrice + "/" + payMethod + "/" + payDetails;
                            send(clientSocket, payRequest.c_str(), payRequest.length(), 0);
                            memset(buffer, 0, BUFFER_SIZE);
                            recv(clientSocket, buffer, BUFFER_SIZE, 0);
                            string payResponse(buffer);
                            istringstream payResponseStream(payResponse);
                            string payResponseCode;
                            getline(payResponseStream, payResponseCode, '/');
                            if (payResponseCode == "341" || payResponseCode == "342"){
                                cout << "You have paid " << ticketPrice << " successfully with " << payMethod << " " << payDetails << endl;
                                cout << "Ticket booked successfully! Your ticket ID is " << ticketId << endl;
                                continue;
                            } else {
                                cout << "Error payment for ticket." << endl;
                                continue;
                            }
                            continue;
                        } else if (bookResponseCode == "431"){
                            cout << "Invalid seat class. Please choose A or B." << endl;
                            continue;
                        } else if (bookResponseCode == "432"){
                            cout << "No flight found with that ID." << endl;
                            continue;
                        } else if (bookResponseCode == "433"){
                            cout << "No seats left for that seat class." << endl;
                            continue;
                        } else if (bookResponseCode == "434"){
                            cout << "Error booking ticket." << endl;
                            continue;
                        } else {
                            cout << "Unexpected server response: " << bookResponse << endl;
                            continue;
                        }
                    } else {
                        cout << "Invalid choice!" << endl;
                    }
                }
            } else if (response == "401/") {
                cout << "Username already exists. Please try again." << endl;
            } else if (response == "402/") {
                cout << "Login failed. Please check your username and password." << endl;
            }
            close(clientSocket);
            cout << "Connection closed." << endl;
        }
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
