#include "server.h"

sqlite3 *db;

int main() {
    // Initialize SQLite in multi-threaded mode
    if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD) != SQLITE_OK) {
        cerr << "Failed to set SQLite to multi-threaded mode." << endl;
        return 1;
    }

    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    // Create server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        cerr << "Error creating server socket" << endl;
        return 1;
    }

    // Configure server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind server socket
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        cerr << "Error binding server socket" << endl;
        close(serverSocket);
        return 1;
    }

    // Start listening for connections
    if (listen(serverSocket, SOMAXCONN) == -1) {
        cerr << "Error listening on server socket" << endl;
        close(serverSocket);
        return 1;
    }

    cout << "Server listening on port " << PORT << "..." << endl;

    // Accept client connections in a loop
    while (true) {
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);
        if (clientSocket == -1) {
            cerr << "Error accepting client connection" << endl;
            continue;
        }

        // Log client connection details
        char clientIP[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN) == NULL) {
            cerr << "Error converting IP address" << endl;
            close(clientSocket);
            continue;
        }
        cout << "Connection from " << clientIP << endl;

        {
            std::lock_guard<std::mutex> lock(clientNotifMapMutex);
            clientNotifMap[clientSocket] = "";
        }

        // Handle client in a separate thread
        thread clientThread(connectClient, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}

void connectClient(int clientSocket) {
    char buffer[BUFFER_SIZE];

    // Open SQLite database
    if (sqlite3_open("Server/flight_database.db", &db) != SQLITE_OK) {
        cerr << "Error opening database: " << sqlite3_errmsg(db) << endl;
        close(clientSocket);
        return;
    }

    // Set database journal mode to WAL for performance
    char *errMsg;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, 0, &errMsg) != SQLITE_OK) {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }

    cout << "Client connected." << endl;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            break;
        }
        buffer[bytesReceived] = '\0';

        string received(buffer);
        cout << "Received: " << received << endl;

        if (received == "exit") {
            break;
        }

        vector<string> requestParts = splitString(received, '/');

        // Handle requests based on the first part of the received message
        if (requestParts[0] == "login") {
            logIn(clientSocket, requestParts[1], requestParts[2]);
        } else if (requestParts[0] == "register") {
            registerUser(clientSocket, requestParts[1], requestParts[2]);
        } 
    }

    cout << "Client disconnected." << endl;
    sqlite3_close(db);
    close(clientSocket);
}

void logIn(int clientSocket, const string &username, const string &password){
    sqlite3_stmt *stmt;
    string query = "SELECT username, password FROM Users WHERE username = ? AND password = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User user;
        user.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        user.password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        std::cout << "Send: 220/" << user.username << endl;
        send(clientSocket, "220/", strlen("220/"), 0);
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            userSocketMap[username] = clientSocket;
        }
        sqlite3_finalize(stmt);
        handleUserFunctions(clientSocket, user);
    } else {
        std::cout << "Send: 402/" << username << endl; 
        sqlite3_finalize(stmt);
        send(clientSocket, "402/", strlen("402/"), 0);
    }
}

void registerUser(int clientSocket, const string &username, const string &password) {
    sqlite3_stmt *stmt;
    string query = "SELECT username FROM Users WHERE username = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::cout << "Send: 401/" << username << endl;
        sqlite3_finalize(stmt);
        send(clientSocket, "401/", strlen("401/"), 0);
    } else {
        query = "INSERT INTO Users (username, password) VALUES (?, ?)";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            cerr << "Error inserting user data: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
        }
        User newUser;
        newUser.username = username;
        newUser.password = password;
        std::cout << "210/" << newUser.username << endl;
        send(clientSocket, "210/", strlen("210/"), 0);
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            userSocketMap[username] = clientSocket;
        }
        sqlite3_finalize(stmt);
        handleUserFunctions(clientSocket, newUser);
    }
}

void searchFlight1(int clientSocket, const string &departurePoint, const string &destinationPoint, const User &user) {
    string notification = checkNotifications(clientSocket);
    string msg;
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "311/";
  
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        msg = "411/" + notification;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departurePoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destinationPoint.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        Flight flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.numSeatsA = sqlite3_column_int(stmt, 2);
        flight.numSeatsB = sqlite3_column_int(stmt, 3);
        flight.priceA = sqlite3_column_int(stmt, 4);
        flight.priceB = sqlite3_column_int(stmt, 5);
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flightId + ",";
        result_str += to_string(flight.numSeatsA) + ",";
        result_str += to_string(flight.numSeatsB) + ",";
        result_str += to_string(flight.priceA) + " VND" + ",";
        result_str += to_string(flight.priceB) + " VND" + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ";";
    }

    if (!found) {
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        msg = result_str + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void searchFlight2(int clientSocket, const string &departurePoint, const string &destinationPoint, const string &departureDate, const User &user) {
    string msg;
    string notification = checkNotifications(clientSocket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "312/";
    
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departurePoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destinationPoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departureDate.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        Flight flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.numSeatsA = sqlite3_column_int(stmt, 2);
        flight.numSeatsB = sqlite3_column_int(stmt, 3);
        flight.priceA = sqlite3_column_int(stmt, 4);
        flight.priceB = sqlite3_column_int(stmt, 5);
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flightId + ",";
        result_str += to_string(flight.numSeatsA) + ",";
        result_str += to_string(flight.numSeatsB) + ",";
        result_str += to_string(flight.priceA) + " VND" + ",";
        result_str += to_string(flight.priceB) + " VND" + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ";";
    }

    if (!found) {
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        msg = result_str + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void searchFlight3(int clientSocket, const string &departurePoint, const string &destinationPoint, const string &returnDate, const User &user) {
    string msg;
    string notification = checkNotifications(clientSocket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "313/";
    
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departurePoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destinationPoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, returnDate.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        Flight flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.numSeatsA = sqlite3_column_int(stmt, 2);
        flight.numSeatsB = sqlite3_column_int(stmt, 3);
        flight.priceA = sqlite3_column_int(stmt, 4);
        flight.priceB = sqlite3_column_int(stmt, 5);
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flightId + ",";
        result_str += to_string(flight.numSeatsA) + ",";
        result_str += to_string(flight.numSeatsB) + ",";
        result_str += to_string(flight.priceA) + " VND" + ",";
        result_str += to_string(flight.priceB) + " VND" + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ";";
    }

    if (!found) {
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        msg = result_str + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void searchFlight4(int clientSocket, const string &departurePoint, const string &destinationPoint, const string &departureDate, const string &returnDate, const User &user) {
    string msg;
    string notification = checkNotifications(clientSocket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "314/";
    
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departurePoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destinationPoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departureDate.c_str(), -1, SQLITE_STATIC); 
    sqlite3_bind_text(stmt, 4, returnDate.c_str(), -1, SQLITE_STATIC);
 

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        Flight flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.numSeatsA = sqlite3_column_int(stmt, 2);
        flight.numSeatsB = sqlite3_column_int(stmt, 3);
        flight.priceA = sqlite3_column_int(stmt, 4);
        flight.priceB = sqlite3_column_int(stmt, 5);
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flightId + ",";
        result_str += to_string(flight.numSeatsA) + ",";
        result_str += to_string(flight.numSeatsB) + ",";
        result_str += to_string(flight.priceA) + " VND" + ",";
        result_str += to_string(flight.priceB) + " VND" + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ";";
    }

    if (!found) {
        msg = "411/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        msg = result_str + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void compareFlight(int clientSocket, const string& departurePoint, const string& destinationPoint, const string& departureDate, const string& seatClass, const string& order, const User& user) {
    string notification = checkNotifications(clientSocket);
    string msg;
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "320/";
    string seatColumn = (seatClass == "A") ? "seat_class_A" : "seat_class_B";
    string priceColumn = (seatClass == "A") ? "price_A" : "price_B";

    string orderClause = (order == "ASC") ? "ASC" : "DESC";
    string query = "SELECT *"
                   "FROM Flights "
                   "WHERE departure_point = ? "
                   "AND destination_point = ? "
                   "AND departure_date <= ? "
                   "ORDER BY " + priceColumn + " " + orderClause;
    
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "421/", strlen("421/"), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departurePoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destinationPoint.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departureDate.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;
        Flight flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.numSeatsA = sqlite3_column_int(stmt, 2);
        flight.numSeatsB = sqlite3_column_int(stmt, 3);
        flight.priceA = sqlite3_column_int(stmt, 4);
        flight.priceB = sqlite3_column_int(stmt, 5);
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
        result_str += flight.company + ",";
        result_str += flight.flightId + ",";
        result_str += to_string(flight.numSeatsA) + ",";
        result_str += to_string(flight.numSeatsB) + ",";
        result_str += to_string(flight.priceA) + " VND" + ",";
        result_str += to_string(flight.priceB) + " VND" + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ";";
    }

    if (!found) {
        msg = "421/" + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        msg = result_str + notification;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
    sqlite3_finalize(stmt);
}

void bookFlight(int clientSocket, const string &flightId, const string &seatClass, const User &user) {
    string msg;
    sqlite3_stmt *stmt;
    int ticketPrice = 0;
    string queryPrice = "SELECT ";
    queryPrice += (seatClass == "A" ? "price_A" : "price_B");
    queryPrice += " FROM Flights WHERE flight_num = ?";

    sqlite3_prepare_v2(db, queryPrice.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, flightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ticketPrice = sqlite3_column_int(stmt, 0);
    } else {
        cerr << "Failed to retrieve ticket price." << endl;
    }

    string querySeat;
    int availableSeats;

    if (seatClass == "A") {
        querySeat = "SELECT seat_class_A FROM Flights WHERE flight_num = ?";
    } else if (seatClass == "B") {
        querySeat = "SELECT seat_class_B FROM Flights WHERE flight_num = ?";
    } else {
        msg = "431/";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    if (sqlite3_prepare_v2(db, querySeat.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, flightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        availableSeats = sqlite3_column_int(stmt, 0);
    } else {
        msg = "432/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    std::cout << "Seat available: " << availableSeats << endl;
    if (availableSeats == 0) {
        msg = "433/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }
    string query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        msg = "434/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);

        int userId = -1;
        string query1 = "SELECT user_id FROM Users WHERE username = ?";
        if (sqlite3_prepare_v2(db, query1.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                userId = sqlite3_column_int(stmt, 0);
            } else {
                msg = "434/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(clientSocket, msg.c_str(), msg.length(), 0);
                return;
            }
            sqlite3_finalize(stmt);
        } else {
            cerr << "Error preparing user query: " << sqlite3_errmsg(db) << endl;
            msg = "434/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            send(clientSocket, msg.c_str(), msg.length(), 0);
            sqlite3_finalize(stmt);
            return;
        }
        string ticketId = generateTicketId();

        string paymentStatus = "NOT_PAID";
        string query2 = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price,payment) VALUES (?, ?, ?, ?, ?,?)";
        if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, userId);
            sqlite3_bind_text(stmt, 3, flightId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, seatClass.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, ticketPrice);
            sqlite3_bind_text(stmt, 6, paymentStatus.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_DONE) {
                std::cout << "Process booking and redirect to payment\n";
                string bookSuccess = "330/";
                bookSuccess += ticketId + "/";
                bookSuccess += to_string(ticketPrice) + "/";
                msg = bookSuccess;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(clientSocket, msg.c_str(), msg.length(), 0);

                updateSeatCount(db, flightId, seatClass, 1);
                sqlite3_finalize(stmt);
            } else {
                msg = "434/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "Error inserting ticket data: " << sqlite3_errmsg(db) << endl;
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
        } else{
            msg = "434/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing insert query: " << sqlite3_errmsg(db) << endl;
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }
    } else {
        msg = "434/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
}

void updateSeatCount (sqlite3* db, const string &flightId, const string &seatClass, int numSeats) {
    sqlite3_stmt *stmt;
    string query;
    if (seatClass == "A") {
        query = "UPDATE Flights SET seat_class_A = seat_class_A - ? WHERE flight_num = ?";
    } else if (seatClass == "B") {
        query = "UPDATE Flights SET seat_class_B = seat_class_B - ? WHERE flight_num = ?";
    } else {
        cerr << "Invalid seat class." << endl;
        return;
    }

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing update query: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, numSeats);
    sqlite3_bind_text(stmt, 2, flightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "SQL error in updating seat count: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_finalize(stmt);
}

void processPayment(int clientSocket, const string &ticketId, const int &ticketPrice,const string &paymentMethod, const string &paymentDetails, const User &user) {
    string msg;
    sqlite3_stmt *stmt;

    if ((paymentMethod == "Card" && paymentDetails.length() != 16) ||
        (paymentMethod == "E-Wallet" && paymentDetails.length() != 10)) {
        string payResponse = "442/"; // Invalid format
        send(clientSocket, payResponse.c_str(), payResponse.length(), 0);
        return;
    }

    string updatePayment = "UPDATE Tickets SET payment = 'PAID' WHERE ticket_code = ?";

    if (sqlite3_prepare_v2(db, updatePayment.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        msg = "443/";
        cerr << "Error preparing update statement" << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    } else {
        if (sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
            msg = "443/";
            cerr << "Error binding ticket ID to update statement" << endl;
            cout << "Send: " << msg << " ->" << user.username << "\n";
            send(clientSocket, msg.c_str(), msg.length(), 0);
        } else {
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                msg = "443/";
                cerr << "Error executing update statement" << endl;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
        }
        sqlite3_finalize(stmt);
        msg = "341/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
}

void handleUserFunctions(int clientSocket, const User &user) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE, 0);
        if (bytesReceived <= 0) {
            break;
        }
        buffer[bytesReceived] = '\0';

        string received(buffer);
        cout << "Received: " << received << " from " << user.username << endl;

        if (received == "logout") {
            cout << "Send: 230/ ->" << user.username << "\n";
            send(clientSocket, "230/", strlen("230/"), 0);
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap.erase(user.username);
            }
            return;
        }

        if (received == "exit search request") {
            string msg;
            msg += "Exit search";
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }

        vector<string> requestParts = splitString(received, '/');

        if (requestParts[0] == "search1") {
            searchFlight1(clientSocket, requestParts[1], requestParts[2], user);
        } else if (requestParts[0] == "search2") {
            searchFlight2(clientSocket, requestParts[1], requestParts[2], requestParts[3], user);
        } else if (requestParts[0] == "search3") {
            searchFlight3(clientSocket, requestParts[1], requestParts[2], requestParts[3], user);
        } else if (requestParts[0] == "search4") {
            searchFlight4(clientSocket, requestParts[1], requestParts[2], requestParts[3], requestParts[4], user);
        } else if (requestParts[0] == "compare") {
            compareFlight(clientSocket, requestParts[1], requestParts[2], requestParts[3], requestParts[4], requestParts[5], user);
        } else if (requestParts[0] == "book") {
            bookFlight(clientSocket, requestParts[1], requestParts[2], user);
        } else if (requestParts[0] == "pay") {
            processPayment(clientSocket, requestParts[1], stoi(requestParts[2]), requestParts[3], requestParts[4], user);
        }
    }
}
