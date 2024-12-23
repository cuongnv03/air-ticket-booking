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
    if (userSocketMap.find(username) != userSocketMap.end())
    {
        std::cout << "Send: N_login1\n"
                  << endl;
        send(clientSocket, "N_login1", strlen("N_login1"), 0);
        return;
    }

    if (username == "admin" && password == "1")
    {
        std::cout << "Send: Y_admin -> Admin\n";
        send(clientSocket, "Y_admin", strlen("Y_admin"), 0);
        adminMode(clientSocket);
    }
    else{
    sqlite3_stmt *stmt;
    string query = "SELECT user_id, username, password FROM Users WHERE username = ? AND password = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        User user;
        user.userId = sqlite3_column_int(stmt, 0); // Retrieve user_id
        user.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        user.password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
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
            send(clientSocket, "402/", strlen("402/"), 0);
            return;
        }
        sqlite3_finalize(stmt);
        // Retrieve the newly created user_id and proceed
        query = "SELECT user_id FROM Users WHERE username = ? AND password = ?";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            cerr << "Error preparing query to retrieve user_id: " << sqlite3_errmsg(db) << endl;
            send(clientSocket, "402/", strlen("402/"), 0);
            return;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        User newUser;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            newUser.userId = sqlite3_column_int(stmt, 0); // Retrieve user_id
            newUser.username = username;
            newUser.password = password;

            cout << "Send: 210/" << newUser.username << endl;
            send(clientSocket, "210/", strlen("210/"), 0);

            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap[username] = clientSocket;
            }

            sqlite3_finalize(stmt);
            handleUserFunctions(clientSocket, newUser);
        } else {
            cerr << "Error retrieving user_id for the new user: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            send(clientSocket, "402/", strlen("402/"), 0);
        }
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
    // printf("userid: %d\n", user.userId);
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

void processPayment(int clientSocket, const string &ticketId, const int &ticketPrice, const string &paymentMethod, const string &paymentDetails, const User &user) {
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

void cancelTicket(int clientSocket, const std::string& ticketId, const User& user) {
    sqlite3_stmt *stmt;
    string msg;

    // Validate ticket ownership
    string query = "SELECT user_id, flight_num, seat_class, ticket_price FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "493/", strlen("493/"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);

    int userId = -1, ticketPrice = 0;
    string flightId, seatClass;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0);
        flightId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        seatClass = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        ticketPrice = sqlite3_column_int(stmt, 3);
    } else {
        send(clientSocket, "491/", strlen("491/"), 0); // Ticket not found or doesn't belong to user
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    if (userId != user.userId) {
        send(clientSocket, "491/", strlen("491/"), 0);
        return;
    }

    // Notify the client to input payment details for refund
    msg = "390/" + ticketId + "/" + to_string(ticketPrice);
    send(clientSocket, msg.c_str(), msg.length(), 0);
}

void processRefund(int clientSocket, const string& ticketId, const int &ticketPrice, const string& paymentMethod, const string& paymentDetails, const User& user) {
    sqlite3_stmt *stmt;
    string msg;

    // Validate ticket existence
    string query = "SELECT flight_num, seat_class FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "493/", strlen("493/"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);

    string flightId, seatClass;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        flightId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        seatClass = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    } else {
        send(clientSocket, "491/", strlen("491/"), 0); // Ticket not found
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // Validate payment details
    if ((paymentMethod == "Card" && paymentDetails.length() != 16) ||
        (paymentMethod == "E-Wallet" && paymentDetails.length() != 10)) {
        send(clientSocket, "442/", strlen("442/"), 0); // Invalid payment details
        return;
    }

    // Delete ticket and process refund
    string deleteQuery = "DELETE FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, deleteQuery.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing delete query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "493/", strlen("493/"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "Error deleting ticket: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "493/", strlen("493/"), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // Update seat count
    updateSeatCount(db, flightId, seatClass, -1);

    // Refund successful
    send(clientSocket, "391/", strlen("391/"), 0);
}

void changeTicket(int clientSocket, const std::string& ticketId, const std::string& newFlightId, const std::string& seatClass, const User& user) {
    sqlite3_stmt *stmt;
    string msg;

    // Validate ownership of the ticket
    string oldQuery = "SELECT user_id, ticket_price, flight_num, seat_class FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, oldQuery.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "482/", strlen("482/"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);

    int userId = -1, oldPrice = 0;
    string oldFlightId, oldSeatClass;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        userId = sqlite3_column_int(stmt, 0);
        oldPrice = sqlite3_column_int(stmt, 1);
        oldFlightId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        oldSeatClass = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
    } else {
        send(clientSocket, "481/", strlen("481/"), 0); // Ticket not found or not owned by user
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    if (userId != user.userId) {
        send(clientSocket, "481/", strlen("481/"), 0);
        return;
    }

    // Delete ticket
    string deleteQuery = "DELETE FROM Tickets WHERE ticket_code = ?";
    if (sqlite3_prepare_v2(db, deleteQuery.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing delete query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "482/", strlen("482/"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, ticketId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        cerr << "Error deleting ticket: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "482/", strlen("482/"), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);

    // Update seat count
    updateSeatCount(db, oldFlightId, oldSeatClass, -1);

    int newTicketPrice = 0;
    string newQueryPrice = "SELECT ";
    newQueryPrice += (seatClass == "A" ? "price_A" : "price_B");
    newQueryPrice += " FROM Flights WHERE flight_num = ?";

    sqlite3_prepare_v2(db, newQueryPrice.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, newFlightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        newTicketPrice = sqlite3_column_int(stmt, 0);
    } else {
        cerr << "Failed to retrieve ticket price." << endl;
    }

    string newQuerySeat;
    int availableSeats;

    if (seatClass == "A") {
        newQuerySeat = "SELECT seat_class_A FROM Flights WHERE flight_num = ?";
    } else if (seatClass == "B") {
        newQuerySeat = "SELECT seat_class_B FROM Flights WHERE flight_num = ?";
    } else {
        msg = "431/";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    if (sqlite3_prepare_v2(db, newQuerySeat.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, "482/", strlen("482/"), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, newFlightId.c_str(), -1, SQLITE_STATIC);

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
    string newQuery = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, newQuery.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        msg = "482/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, newFlightId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_finalize(stmt);

        int userId = -1;
        string query1 = "SELECT user_id FROM Users WHERE username = ?";
        if (sqlite3_prepare_v2(db, query1.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                userId = sqlite3_column_int(stmt, 0);
            } else {
                msg = "482/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(clientSocket, msg.c_str(), msg.length(), 0);
                return;
            }
            sqlite3_finalize(stmt);
        } else {
            cerr << "Error preparing user query: " << sqlite3_errmsg(db) << endl;
            msg = "482/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            send(clientSocket, msg.c_str(), msg.length(), 0);
            sqlite3_finalize(stmt);
            return;
        }

        string newTicketId = generateTicketId();
        string paymentStatus = "NOT_PAID";
        string query = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price, payment) VALUES (?, ?, ?, ?, ?, ?)";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, newTicketId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, user.userId);
            sqlite3_bind_text(stmt, 3, newFlightId.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, seatClass.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, newTicketPrice);
            sqlite3_bind_text(stmt, 6, paymentStatus.c_str(), -1, SQLITE_STATIC);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                // Calculate price difference
                int priceDifference = newTicketPrice - oldPrice;
                if (priceDifference > 0) {
                    // Notify the client to pay the additional amount
                    cout << "Process change and redirect to payment\n";
                    msg = "381/" + newTicketId + "/" + to_string(priceDifference) + "/";
                    cout << "Send: " << msg << " -> " << user.username << "\n";
                    send(clientSocket, msg.c_str(), msg.length(), 0);
                    updateSeatCount(db, newFlightId, seatClass, 1);
                    sqlite3_finalize(stmt);
                } else {
                    // Notify the client about the refund amount
                    cout << "Process change and redirect to refund\n";
                    msg = "382/" + newTicketId + "/" + to_string(-priceDifference) + "/";
                    cout << "Send: " << msg << " -> " << user.username << "\n";
                    send(clientSocket, msg.c_str(), msg.length(), 0);
                    updateSeatCount(db, newFlightId, seatClass, 1);
                    sqlite3_finalize(stmt);
                }
            } else {
                msg = "482/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "Error inserting ticket data: " << sqlite3_errmsg(db) << endl;
                send(clientSocket, msg.c_str(), msg.length(), 0);
            }
        } else {
            msg = "482/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing insert query: " << sqlite3_errmsg(db) << endl;
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }
    } else {
        msg = "482/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
}

void processPaymentForChange(int clientSocket, const string &ticketId, const int &morePay, const string &paymentMethod, const string &paymentDetails, const User &user) {
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

void processRefundForChange(int clientSocket, const string& ticketId, const int &refundAmount, const string& paymentMethod, const string& paymentDetails, const User& user) {
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
        msg = "391/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
}

void view_ticket(int clientSocket, const User &user) {
    string notification = checkNotifications(clientSocket);
    string message;
    sqlite3_stmt *stmt;

    // Câu truy vấn lấy thông tin vé của người dùng
    string query = "SELECT T.ticket_code, T.flight_num, T.seat_class, T.ticket_price, T.payment, F.company, F.departure_date, F.return_date, F.departure_point, F.destination_point "
                           "FROM Tickets T "
                           "JOIN Flights F ON T.flight_num = F.flight_num "
                           "JOIN Users U ON T.user_id = U.user_id "
                           "WHERE U.username = ?";


    // Chuẩn bị truy vấn
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        message = "451/" + notification;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << message << " -> " << user.username << "\n";
        send(clientSocket, message.c_str(), message.length(), 0);
        return;
    }

    // Gắn username vào truy vấn
      sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);
    // printf("username: %s\n", user.username);
    string result_str = "350/";
    bool found = false;

    // Duyệt qua các kết quả trả về từ truy vấn
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        found = true;

        // Ánh xạ dữ liệu vé và chuyến bay
        Ticket ticket;
        Flight flight;

        ticket.ticketId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        ticket.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        ticket.seatClass = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        ticket.ticketPrice = sqlite3_column_int(stmt, 3);
        ticket.paymentStatus = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));

        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));
        
            stringstream ss;
            ss << ticket.ticketPrice;
            string str = ss.str();
            string str_ticket_price = str.substr(0, 3) + "." + str.substr(3, 3);
     
        // Tạo chuỗi kết quả
        result_str += ticket.ticketId + ",";
        result_str += ticket.flightId + ",";
        result_str += flight.company + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ",";
        result_str += ticket.seatClass + ",";
        result_str += str_ticket_price + " VND,";
        result_str += ticket.paymentStatus + ";";
    }

    // Giải phóng bộ nhớ của câu truy vấn
    sqlite3_finalize(stmt);

    // Gửi kết quả hoặc thông báo không tìm thấy vé
    if (!found) {
        message = "451/" + notification;
        cout << "Send: " << message << " -> " << user.username << "\n";
        cout << user.username << ": Found no ticket\n";
    } else {
        message = result_str + notification + "/";
        cout << "Send: " << message << " -> " << user.username << "\n";
    }

    send(clientSocket, message.c_str(), message.length(), 0);
}

void print_ticket(int clientSocket, const string ticket_code, const User &user)
{
    sqlite3_stmt *stmt;
    string msg;
    string noti = checkNotifications(clientSocket);
    int userId =get_user_id_from_username(user.username);
    string query = "SELECT T.ticket_code, T.flight_num, T.seat_class, T.ticket_price, T.payment, F.company, F.departure_date, F.return_date, F.departure_point, F.destination_point "
                   "FROM Tickets T "
                   "JOIN Flights F ON T.flight_num = F.flight_num "
                   "WHERE T.ticket_code = ? AND T.user_id = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "461/" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, userId);
    string result_str = "360/";
    bool found = false;

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Ticket ticket;
        Flight flight;
        ticket.ticketId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        ticket.flightId = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        ticket.seatClass = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        ticket.ticketPrice = sqlite3_column_int(stmt, 3);
        ticket.paymentStatus = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 5));
        flight.departureDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.returnDate = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departurePoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.destinationPoint = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

                 stringstream ss;
            ss << ticket.ticketPrice;
            string str = ss.str();
            string str_ticket_price = str.substr(0, 3) + "." + str.substr(3, 3);
        result_str += ticket.ticketId + ",";
        result_str += ticket.flightId + ",";
        result_str += flight.company + ",";
        result_str += flight.departurePoint + ",";
        result_str += flight.destinationPoint + ",";
        result_str += flight.departureDate + ",";
        result_str += flight.returnDate + ",";
        result_str += ticket.seatClass + ",";
        result_str += str_ticket_price + " VND" + ",";
        result_str += ticket.paymentStatus + ";";
    }

    sqlite3_finalize(stmt);

    if (!found)
    {
        msg = "461/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
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
        } else if (requestParts[0] == "CANCEL") {
            cancelTicket(clientSocket, requestParts[1], user);
        } else if (requestParts[0] == "REFUND") {
            processRefund(clientSocket, requestParts[1], stoi(requestParts[2]), requestParts[3], requestParts[4], user);
        } else if (requestParts[0] == "CHANGE") {
            changeTicket(clientSocket, requestParts[1], requestParts[2], requestParts[3], user);
        } else if (requestParts[0] == "PAYC") {
            processPaymentForChange(clientSocket, requestParts[1], stoi(requestParts[2]), requestParts[3], requestParts[4], user);
        } else if (requestParts[0] == "REFUNDC") {
            processRefundForChange(clientSocket, requestParts[1], stoi(requestParts[2]), requestParts[3], requestParts[4], user);
        }
        else if (requestParts[0] == "view") {
            view_ticket(clientSocket, user);
        }
        else if (requestParts[0] == "print") {
            print_ticket(clientSocket, requestParts[1], user);
        }
        
    }
}
int get_user_id_from_username(const std::string &username) {
    sqlite3_stmt *stmt;
    int user_id = -1; // Giá trị mặc định nếu không tìm thấy user
    std::string query = "SELECT user_id FROM Users WHERE username = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            user_id = sqlite3_column_int(stmt, 0);
        } else {
            std::cerr << "User not found for username: " << username << std::endl;
        }

        sqlite3_finalize(stmt);
    } else {
        std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
    }

    return user_id;
}

void updateFlight3(int client_socket, string &flight_num, const string &new_departure_date, const string &new_return_date)
{
    if (!flightIdExists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        std::cout << "Send: N_modify ->Admin\n";
        send(client_socket, "N_modify", strlen("N_modify"), 0);
        return;
    }
    vector<int> affected_ids = getAffectedUserId(flight_num);
    pair<string, string> old = getOldDates(flight_num);
    DateDifference diff = calculateDateDifference(old.first, new_departure_date);
    string noti1;
    if (diff.days != 0)
    {
        noti1 = "Schedule changed: " + flight_num + "'s departure date has changed to " + new_departure_date + " &";
    }
    else if (diff.hours != 0 && diff.days == 0)
    {
        stringstream ss;
        ss << diff.hours;
        string num_hours = ss.str();
        noti1 = "Flight " + flight_num + " has been delayed for " + num_hours + "hours" + " &";
    }
    noti1 += new_return_date;
    sqlite3_stmt *stmt;
    string query = "UPDATE Flights SET departure_date = ?, return_date = ? WHERE flight_num = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, new_return_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            std::lock_guard<std::mutex> lockMap(mapMutex);
            auto it = userSocketMap.find(username);
            if (it != userSocketMap.end())
            {
                int user_socket = it->second;
                {
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += "Y_modified3" + noti1;
                }
            }
            else
            {
                cerr << "User " << username << " not connected for notification." << endl;
            }
        }
        std::cout << "Send: Y_modify ->Admin\n";
        send(client_socket, "Y_modify", strlen("Y_modify"), 0);
    }
    sqlite3_finalize(stmt);
}
void adminMode(int clientSocket){
    char buffer[BUFFER_SIZE];
    int bytes_received;
    while (true)
    {
        bytes_received = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0)
        {
            break;
        }

        buffer[bytes_received] = '\0';
        string received(buffer);
        cout << "Received: " << received << " from admin\n";
        if (toLower(received) == "logout")
        {
            std::cout << "Admin logged out requested" << endl;
            send(clientSocket, "O_log", strlen("O_log"), 0);
            return;
        }
        vector<string> type1 = splitString(received, '/');
        if (toLower(type1[0]) == "add_flight")//add new flight admin
        {
            addFlight(clientSocket, type1[1], type1[2], stoi(type1[3]), stoi(type1[4]), stoi(type1[5]), stoi(type1[6]), type1[7], type1[8], type1[9], type1[10]);
        }
        else if (toLower(type1[0]) == "del_flight")// delete flight admin
        {
        deleteFlight(clientSocket, type1[1]);
        }
        else if (toLower(type1[0]) == "modify1")
        {
            updateFlight1(clientSocket, type1[1], type1[2]);
        }
        else if (toLower(type1[0]) == "modify2")
        {
            updateFlight2(clientSocket, type1[1], type1[2]);
        }
        else if (toLower(type1[0]) == "modify3")
        {
            updateFlight3(clientSocket, type1[1], type1[2], type1[3]);
        }
    }
}
bool flightIdExists(const string &flight_num)
{
    cout << "Checking flight number: " << flight_num << endl;
    sqlite3_stmt *stmt;
    string query = "SELECT 1 FROM Flights WHERE flight_num = ? LIMIT 1";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing select statement: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);
        return true;
    }
    else
    {
        cerr << "Flight number not found or error executing select statement: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }
}

void updateFlight1(int clientSocket, const string &flight_num, const string &new_departure_date)
{
    if (!flightIdExists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        std::cout << "Send: N_modify ->Admin\n";
        send(clientSocket, "N_modify", strlen("N_modify"), 0);
        return;
    }
    vector<int> affected_ids = getAffectedUserId(flight_num);
    pair<string, string> old = getOldDates(flight_num);
    DateDifference diff = calculateDateDifference(old.first, new_departure_date);
    string noti1;
    if (diff.days != 0)
    {
        noti1 = "Schedule changed: " + flight_num + "'s departure date has changed to " + new_departure_date + " &";
    }
    else if (diff.hours != 0 && diff.days == 0)
    {
        stringstream ss;
        ss << diff.hours;
        string num_hours = ss.str();
        noti1 = "Flight " + flight_num + " has been delayed for " + num_hours + "hours" + " &";
    }

    sqlite3_stmt *stmt;

    string query = "UPDATE Flights SET departure_date = ? WHERE flight_num = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            std::lock_guard<std::mutex> lockMap(mapMutex);
            auto it = userSocketMap.find(username);
            if (it != userSocketMap.end())
            {
                int user_socket = it->second;

                {
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += "Y_modified1" + noti1;
                }
            }
            else
            {
                cerr << "User " << username << " not connected for notification." << endl;
            }
        }
        std::cout << "Send: Y_modify ->Admin\n";
        send(clientSocket, "Y_modify", strlen("Y_modify"), 0);
    }
    sqlite3_finalize(stmt);
}
void updateFlight2(int client_socket, string &flight_num, const string &new_return_date)
{
    vector<int> affected_ids = getAffectedUserId(flight_num);
    if (!flightIdExists(flight_num))
    {
        cout << "Flight number does not exist: " << flight_num << endl;
        std::cout << "Send: N_modify ->Admin\n";
        send(client_socket, "N_modify", strlen("N_modify"), 0);
        return;
    }
    sqlite3_stmt *stmt;
    string query = "UPDATE Flights SET return_date = ? WHERE flight_num = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing update statement: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, new_return_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error executing update statement: " << sqlite3_errmsg(db) << endl;
    }
    else
    {
        for (int user_id : affected_ids)
        {
            string username = get_username_from_id(user_id);
            std::lock_guard<std::mutex> lockMap(mapMutex);
            auto it = userSocketMap.find(username);
            if (it != userSocketMap.end())
            {
                int user_socket = it->second;
                std::string notifMessage;
                notifMessage = "Y_modified2Your flight " + flight_num + "'s return date has changed into " + new_return_date + "&";
                {
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += notifMessage;
                }
            }
            else
            {
                cerr << "User " << username << " not connected for notification." << endl;
            }
        }

        std::cout << "Send: Y_modify ->Admin\n";
        send(client_socket, "Y_modify", strlen("Y_modify"), 0);
    }
    sqlite3_finalize(stmt);
}


void addFlight(int clientSocket, const string &company, const string &flight_num, const int &seat_class_A, const int &seat_class_B, const int &price_A, const int &price_B, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date){
    sqlite3_stmt *stmt;
    string msg;
    string check_query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, check_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        msg = "N_add";
        cout << "Send: " << msg << " ->" << "Admin\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        cerr << "Flight number already exists" << endl;
        msg = "N_add";
        cout << "Send: " << msg << " ->" << "Admin\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    string insert_query = "INSERT INTO Flights (company, flight_num, seat_class_A, seat_class_B, price_A, price_B, departure_point, destination_point, departure_date, return_date) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(db, insert_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        msg = "N_add";
        cout << "Send: " << msg << " ->" << "Admin\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, company.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 3, seat_class_A);
    sqlite3_bind_int(stmt, 4, seat_class_B);
    sqlite3_bind_int(stmt, 5, price_A);
    sqlite3_bind_int(stmt, 6, price_B);
    sqlite3_bind_text(stmt, 7, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, return_date.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error inserting data: " << sqlite3_errmsg(db) << endl;
        msg = "N_add";
        cout << "Send: " << msg << " ->" << "Admin\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        cout << "Admin: Flight inserted\n";
        msg = "Y_add";
        cout << "Send: " << msg << " ->" << "Admin\n";
        send(clientSocket, msg.c_str(), msg.length(), 0);
    }
    sqlite3_finalize(stmt);
}
void deleteFlight(int clientSocket, const string &flight_num){
    sqlite3_stmt *stmt;
    const char *flight_check_sql = "SELECT COUNT(*) FROM Flights WHERE flight_num = ?;";
    if (sqlite3_prepare_v2(db, flight_check_sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        cerr << "Error preparing flight check statement" << endl;
        return;
    }
    else
    {
        sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            if (sqlite3_column_int(stmt, 0) == 0)
            {
                std::cout << "Send: N_del ->Admin\n";
                send(clientSocket, "N_del", strlen("N_del"), 0);
                sqlite3_finalize(stmt);
                return;
            }
        }
        sqlite3_finalize(stmt);
    }
    vector<int> affected_user_ids = getAffectedUserId(flight_num);
    // Delete the flight
    string delete_query = "DELETE FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, delete_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        std::cout << "Send: N_del ->Admin\n";
        send(clientSocket, "N_del", strlen("N_del"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error deleting flight: " << sqlite3_errmsg(db) << endl;
        std::cout << "Send: N_del ->Admin\n";
        send(clientSocket, "N_del", strlen("N_del"), 0);
    }
    else
    {
        std::cout << "Flight deleted successfully" << endl;
        std::cout << "Send: Y_del ->Admin\n";
        send(clientSocket, "Y_del", strlen("Y_del"), 0);
        for (int user_id : affected_user_ids)
        {
            string username = get_username_from_id(user_id);
            std::lock_guard<std::mutex> lockMap(mapMutex);
            auto it = userSocketMap.find(username);
            if (it != userSocketMap.end())
            {
                int user_socket = it->second;

                std::string notifMessage = "Y_notif_cancelled" + flight_num;
                {
                    std::lock_guard<std::mutex> lockNotif(clientNotifMapMutex);
                    clientNotifMap[user_socket] += notifMessage;
                }
            }
            else
            {
                cerr << "User " << username << " not connected for notification." << endl;
            }
        }
    }
    sqlite3_finalize(stmt);

    string del_ticket_query = "DELETE FROM Tickets WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, del_ticket_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        std::cout << "Send: N_del ->Admin\n";
        send(clientSocket, "N_del", strlen("N_del"), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "Error deleting tickets: " << sqlite3_errmsg(db) << endl;
        std::cout << "Send: N_del ->Admin\n";
        send(clientSocket, "N_del", strlen("N_del"), 0);
    }
    else
    {
        std::cout << "Tickets for flight " << flight_num << " deleted successfully" << endl;
    }
    sqlite3_finalize(stmt);
}
std::vector<int> getAffectedUserId(const std::string &flight_num)
{
    std::vector<int> affected_user_ids;
    sqlite3_stmt *stmt;

    std::string ticket_check_query = "SELECT user_id FROM Tickets WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, ticket_check_query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        std::cerr << "Error preparing query: " << sqlite3_errmsg(db) << std::endl;
        return affected_user_ids;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        affected_user_ids.push_back(sqlite3_column_int(stmt, 0));
    }
    sqlite3_finalize(stmt);

    return affected_user_ids;
}
std::string get_username_from_id(int user_id)
{
    sqlite3_stmt *stmt;
    std::string username;
    std::string query = "SELECT username FROM Users WHERE user_id = ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
    {
        sqlite3_bind_int(stmt, 1, user_id);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            const unsigned char *username_raw = sqlite3_column_text(stmt, 0);
            username = reinterpret_cast<const char *>(username_raw);
        }
        else
        {
            std::cerr << "User not found for ID: " << user_id << std::endl;
        }

        sqlite3_finalize(stmt);
    }
    else
    {
        std::cerr << "SQL error: " << sqlite3_errmsg(db) << std::endl;
    }

    return username;
}
pair<string, string> getOldDates(const string &flight_num)
{
    sqlite3_stmt *stmt;
    string query = "SELECT departure_date, return_date FROM Flights WHERE flight_num = ?";
    pair<string, string> old_dates;

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing statement: " << sqlite3_errmsg(db) << endl;
        return old_dates;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        old_dates.first = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        old_dates.second = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    }

    sqlite3_finalize(stmt);
    return old_dates;
}