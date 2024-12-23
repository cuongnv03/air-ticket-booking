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
                        tmpNotification = " ";
                    }

                    printUserFunctions();
                    string userChoice;
                    getline(cin, userChoice);
                    string trimmedUserChoice = trimString(toLower(userChoice));

                    if (trimmedUserChoice == "8") { // Logout
                        send(clientSocket, "logout", strlen("logout"), 0);
                        // memset(buffer, 0, BUFFER_SIZE);
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
                    } else if (trimmedUserChoice == "3") {//view ticket
                        string viewRequest = "view/";
                        send(clientSocket, viewRequest.c_str(), viewRequest.length(), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        string viewResponse(buffer);
                        istringstream viewResponseStream(viewResponse);
                        string viewResponseCode, ticketData;
                        getline(viewResponseStream, viewResponseCode, '/');

                        // string response1 = string(buffer);
                        // if (response1.find("Y_notif_cancelled") != std::string::npos)
                        //     {
                        //         size_t pos = response1.find("Y_notif_cancelled");
                        //         tmpNotification += "Your flight " + response1.substr(pos + 17, 6) + " has been cancelled\n";
                        //     }

                        if (viewResponseCode == "350"){
                            getline(viewResponseStream, ticketData, '/');
                            cout << "Your tickets: " << endl;
                            displayTicketInformation(ticketData);
                            
                        } else if (viewResponseCode == "451"){
                            cout << "No tickets found." << endl; 
                        } else {
                            cout << "Unexpected server response: " << viewResponse << endl;
                        }
                    } else if (trimmedUserChoice == "4") { // Cancel Ticket
                        string ticketId;
                        cout << "Enter the ticket ID to cancel: ";
                        getline(cin, ticketId);

                        string cancelRequest = "CANCEL/" + ticketId;
                        send(clientSocket, cancelRequest.c_str(), cancelRequest.length(), 0);

                        memset(buffer, 0, BUFFER_SIZE);
                        recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        string cancelResponse(buffer);
                        istringstream cancelResponseStream(cancelResponse);
                        string cancelResponseCode, ticketPrice;
                        getline(cancelResponseStream, cancelResponseCode, '/');

                        if (cancelResponseCode == "390") {
                            // Parse cancellation response
                            getline(cancelResponseStream, ticketId, '/');
                            getline(cancelResponseStream, ticketPrice, '/');

                            // Prompt user for refund details
                            string paymentMethod, paymentDetails;
                            cout << "To confirm cancellation of flight ticket with ticket ID: "
                            << ticketId << ", please provide refund details.\n";
                            cout << "Ticket Price to be refunded: " << ticketPrice << " VND\n";
                            cout << "Enter payment method (Card/E-Wallet): ";
                            getline(cin, paymentMethod);
                            
                            cout << "Enter payment details: ";
                            getline(cin, paymentDetails);

                            string refundRequest = "REFUND/" + ticketId + "/" + ticketPrice + "/"+ paymentMethod + "/" + paymentDetails;
                            send(clientSocket, refundRequest.c_str(), refundRequest.length(), 0);

                            memset(buffer, 0, BUFFER_SIZE);
                            recv(clientSocket, buffer, BUFFER_SIZE, 0);
                            string refundResponse(buffer);

                            if (refundResponse == "391/") {
                                cout << "Your ticket has been successfully canceled and refunded." << endl;
                            } else if (refundResponse == "442/") {
                                cout << "Invalid refund details. Please try again." << endl;
                            } else if (refundResponse == "443/") {
                                cout << "Error processing refund. Please try again." << endl;
                            } else {
                                cout << "Unexpected server response: " << refundResponse << endl;
                            }
                        } else if (cancelResponse == "491/") {
                            cout << "You do not own this ticket or it does not exist." << endl;
                        } else if (cancelResponse == "493/") {
                            cout << "Invalid ticket cancellation request." << endl;
                        } else {
                            cout << "Unexpected server response: " << cancelResponse << endl;
                        }
                    } else if (trimmedUserChoice == "5") { // Change Ticket
                        string ticketId, newFlightId, seatClass;
                        cout << "Enter your current ticket ID: ";
                        getline(cin, ticketId);
                        cout << "Enter new flight ID: ";
                        getline(cin, newFlightId);
                        cout << "Enter seat class (A/B): ";
                        getline(cin, seatClass);

                        string changeRequest = "CHANGE/" + ticketId + "/" + newFlightId + "/" + seatClass;
                        send(clientSocket, changeRequest.c_str(), changeRequest.length(), 0);

                        memset(buffer, 0, BUFFER_SIZE);
                        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        if (bytesReceived <= 0){
                            break;
                        }
                        buffer[bytesReceived] = '\0';
                        string changeResponse(buffer);
                        istringstream changeResponseStream(changeResponse);
                        string changeResponseCode, priceDiff;
                        getline(changeResponseStream, changeResponseCode, '/');

                        if (changeResponseCode == "381") {
                            // Parse the additional payment request
                            getline(changeResponseStream, ticketId, '/');
                            getline(changeResponseStream, priceDiff, '/');

                            cout << "You need to pay an additional " << priceDiff << " VND to change your ticket with new ticket ID: "
                            << ticketId <<".\n";
                            string paymentMethod, paymentDetails;
                            cout << "Enter payment method (Card/E-Wallet): ";
                            getline(cin, paymentMethod);
                            
                            cout << "Enter payment details: ";
                            getline(cin, paymentDetails);

                            string payRequest = "PAYC/" + ticketId + "/" + priceDiff + "/" + paymentMethod + "/" + paymentDetails;
                            send(clientSocket, payRequest.c_str(), payRequest.length(), 0);

                            memset(buffer, 0, BUFFER_SIZE);
                            recv(clientSocket, buffer, BUFFER_SIZE, 0);
                            string payResponse(buffer);
                            istringstream payResponseStream(payResponse);
                            string payResponseCode;
                            getline(payResponseStream, payResponseCode, '/');
                            if (payResponseCode == "341" || payResponseCode == "342"){
                                cout << "You have paid more " << priceDiff << " successfully with " << paymentMethod << " " << paymentDetails << endl;
                                cout << "Ticket changed successfully! Your new ticket ID is " << ticketId << endl;
                                
                            } else {
                                cout << "Error payment for ticket." << endl;
                                
                            }
                            
                        } else if (changeResponseCode == "382") {
                            // Parse the additional payment request
                            getline(changeResponseStream, ticketId, '/');
                            getline(changeResponseStream, priceDiff, '/');
                            cout << "You will be refunded " << priceDiff << " VND for changing your ticket with new ticket ID: "
                            << ticketId << ".\n";
                            string paymentMethod, paymentDetails;
                            cout << "Enter refund method (Card/E-Wallet): ";
                            getline(cin, paymentMethod);
                            
                            cout << "Enter refund details: ";
                            getline(cin, paymentDetails);

                            string refundRequest = "REFUNDC/" + ticketId + "/" + priceDiff + "/" + paymentMethod + "/" + paymentDetails;
                            send(clientSocket, refundRequest.c_str(), refundRequest.length(), 0);

                            memset(buffer, 0, BUFFER_SIZE);
                            recv(clientSocket, buffer, BUFFER_SIZE, 0);
                            string refundResponse(buffer);
                            istringstream refundResponseStream(refundResponse);
                            string refundResponseCode;
                            getline(refundResponseStream, refundResponseCode, '/');
                            if (refundResponseCode == "391" || refundResponseCode == "392"){
                                cout << "You have been refunded " << priceDiff << " successfully with " << paymentMethod << " " << paymentDetails << endl;
                                cout << "Ticket changed successfully! Your new ticket ID is " << ticketId << endl;
                                
                            } else {
                                cout << "Error payment for ticket." << endl;
                                
                            }
                            
                        } else if (changeResponseCode == "481") {
                            cout << "You do not own this ticket or it does not exist." << endl;
                        } else if (changeResponseCode == "482") {
                            cout << "Invalid input for ticket change." << endl;
                        } else {
                            cout << "Unexpected server response: " << changeResponse << endl;
                        }
                    } else if(trimmedUserChoice == "6") {//print ticket
                        cout << "Enter ticket code: ";
                        string ticket_code;
                        getline(cin, ticket_code);
                        string printRequest = "print/" + ticket_code;
                        send(clientSocket, printRequest.c_str(), printRequest.length(), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        string printResponse(buffer);
                        istringstream printResponseStream(printResponse);
                        string printResponseCode;
                        getline(printResponseStream, printResponseCode, '/');
                        if (printResponseCode == "360"){
                            string ticketData = printResponse.substr(4);
                            cout << "Ticket saved: " << endl;
                            save_tickets_to_file(ticketData, ticket_code);
                        } else if (printResponseCode == "461"){
                            cout << "Fail to print." << endl;
                        } else {
                            cout << "Unexpected server response: " << printResponse << endl;
                        }
                    } else {
                        cout << "Invalid choice!" << endl;
                    }
                    string response1 = string(buffer);
                    if (response1.find("Y_notif_cancelled") != std::string::npos)
                    {
                        size_t pos = response1.find("Y_notif_cancelled");
                        tmpNotification += "Your flight " + response1.substr(pos + 17, 6) + " has been cancelled\n";
                    }
                    if (response1.find("Y_modified1") != std::string::npos)
                    {
                        size_t startPos = 0;
                        while ((startPos = response1.find("Y_modified1", startPos)) != std::string::npos)
                        {
                            size_t endPos = response1.find('&', startPos);
                            if (endPos != std::string::npos)
                            {
                                tmpNotification += response1.substr(startPos + 11, endPos - (startPos + 12)) + "\n";
                                startPos = endPos + 1;
                            }
                        }
                    }
                    if (response1.find("Y_modified2") != std::string::npos)
                    {
                        size_t startPos = 0;
                        while ((startPos = response1.find("Y_modified2", startPos)) != std::string::npos)
                        {
                            size_t endPos = response1.find('&', startPos);
                            if (endPos != std::string::npos)
                            {
                                tmpNotification += response1.substr(startPos + 11, endPos - (startPos + 12)) + "\n";
                                startPos = endPos + 1;
                            }
                        }
                    }
                }
            } else if (response == "401/") {
                cout << "Username already exists. Please try again." << endl;
            } else if (response == "402/") {
                cout << "Login failed. Please check your username and password." << endl;
            } else if (response == "Y_admin")
            {
                string admin_choice;
                currentRole = Role::Admin;
                std::cout << "You're the admin\n";
                while (true)
                {
                    print_admin_menu();
                    getline(cin, admin_choice);
                    string lower_choice2 = trimString(toLower(admin_choice));

                    if (lower_choice2 == "1")//add flight
                    {
                        string company, flight_num, seat_class_A, seat_class_B, seat_class_A_price, seat_class_B_price, departure_point, destination_point, departure_date, return_date;
                        std::cout << "Enter the company: ";
                        getline(cin, company);

                        std::cout << "Enter the flight number: ";
                        getline(cin, flight_num);

                        std::cout << "Enter the number of seat left for seat class A: ";
                        getline(cin, seat_class_A);

                        std::cout << "Enter the number of seat left for seat class B: ";
                        getline(cin, seat_class_B);

                        std::cout << "Enter the price for seat class A: ";
                        getline(cin, seat_class_A_price);

                        std::cout << "Enter the price for seat class B: ";
                        getline(cin, seat_class_B_price);

                        std::cout << "Enter the departure point: ";
                        getline(cin, departure_point);

                        std::cout << "Enter the destination point: ";
                        getline(cin, destination_point);

                        std::cout << "Enter the departure date: ";
                        getline(cin, departure_date);

                        std::cout << "Enter the return date: ";
                        getline(cin, return_date);
                        string add_msg = "add_flight/" + company + "/" + flight_num + "/" + seat_class_A + "/" + seat_class_B + "/" + seat_class_A_price + "/" + seat_class_B_price + "/" + departure_point + "/" + destination_point + "/" + departure_date + "/" + return_date;
                        send(clientSocket, add_msg.c_str(), add_msg.length(), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        int bytes_received2 = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        if (bytes_received2 <= 0)
                        {
                            break;
                        }
                        buffer[bytes_received2] = '\0';

                        string response2 = string(buffer);

                        if (response2 == "Y_add")
                        {
                            std::cout << "Successfully inserted\n";
                        }
                        else if (response2 == "N_add")
                        {
                            std::cout << "The flight number might already exist\n";
                        }
                    }
                    else if (lower_choice2 == "2")//delete flight
                    {
                        string del_flight_num;
                        std::cout << "Enter the flight number which you want to delete: ";
                        getline(cin, del_flight_num);
                        string ad_del_msg = "del_flight/" + del_flight_num;
                        send(clientSocket, ad_del_msg.c_str(), ad_del_msg.length(), 0);
                        printf("%s", buffer);
                        memset(buffer, 0, BUFFER_SIZE);
                        int bytes_received2 = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        if (bytes_received2 <= 0)
                        {
                            break;
                        }
                        buffer[bytes_received2] = '\0';

                        string response2 = string(buffer);

                        if (response2 == "N_del")
                        {
                            std::cout << "The flight_num does not exist\n";
                        }
                        else if (response2.find("Y_del") == 0)
                        {
                            std::cout << "Deleted \n";
                        }
                    }
                    else if (lower_choice2 == "3")//modify flight
                    {
                        string modify_flight_num, modify_departure_date, modify_return_date;
                        string modify_msg;

                        std::cout << "Enter the flight number you want to modify: ";
                        getline(cin, modify_flight_num);
                        std::cout << "Enter the new departure date (possibly blank): ";
                        getline(cin, modify_departure_date);
                        std::cout << "Enter the new return date (possibly blank): ";
                        getline(cin, modify_return_date);
                        if (!modify_departure_date.empty() && !modify_return_date.empty())
                        {
                            modify_msg += "modify3/" + modify_flight_num + "/" + modify_departure_date + "/" + modify_return_date;
                        }
                        if (!modify_departure_date.empty() && modify_return_date.empty())
                        {
                            modify_msg += "modify1/" + modify_flight_num + "/" + modify_departure_date;
                        }
                        if (modify_departure_date.empty() && !modify_return_date.empty())
                        {
                            modify_msg += "modify2/" + modify_flight_num + "/" + modify_return_date;
                        }
                        send(clientSocket, modify_msg.c_str(), modify_msg.length(), 0);
                        int bytes_received2 = recv(clientSocket, buffer, BUFFER_SIZE, 0);
                        if (bytes_received2 <= 0)
                        {
                            break;
                        }
                        buffer[bytes_received2] = '\0';

                        string response2 = string(buffer);

                        if (response2 == "N_modify")
                        {
                            std::cout << "Can't find the flight number\n";
                        }
                        else if (response2 == "Y_modify")
                        {
                            std::cout << "Modified successfully\n";
                        }
                    }
                }
            }
        }
        close(clientSocket);
        cout << "Connection closed." << endl;
    } catch (const exception &e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
