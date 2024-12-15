#include "server.h"

sqlite3 *db;

int main()
{
    if (sqlite3_config(SQLITE_CONFIG_MULTITHREAD) != SQLITE_OK)
    {
        cerr << "Failed to set SQLite to multi-threaded mode." << endl;
        return 1;
    }
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        cerr << "Error creating server socket" << endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        cerr << "Error binding server socket" << endl;
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, SOMAXCONN) == -1)
    {
        cerr << "Error listening on server socket" << endl;
        close(server_socket);
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "..." << endl;

    while (true)
    {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1)
        {
            cerr << "Error accepting client connection" << endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL)
        {
            cerr << "Error converting IP address" << endl;
            close(client_socket);
            continue;
        }
        std::cout << "Received request from " << client_ip << endl;
        {
            std::lock_guard<std::mutex> lock(clientNotifMapMutex);
            clientNotifMap[client_socket] = "";
        }
        thread client_thread(connect_client, client_socket);
        client_thread.detach();
    }

    close(server_socket);
    return 0;
}

void connect_client(int client_socket)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    if (sqlite3_open("Server/flight_database.db", &db) != SQLITE_OK)
    {
        cerr << "Error opening database: " << sqlite3_errmsg(db) << endl;
        close(client_socket);
        return;
    }
    char *errMsg;
    if (sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, 0, &errMsg) != SQLITE_OK)
    {
        cerr << "SQL error: " << errMsg << endl;
        sqlite3_free(errMsg);
    }
    std::cout << "Connected to client" << endl;

    while (true)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        string received(buffer);
        cout << "Received: " << received << "\n";
        if (received == "exit")
        {
            break;
        }

        vector<string> type1 = split(received, '/');

        if (type1[0] == "login")
        {
            log_in(client_socket, type1[1], type1[2]);
        }
        else if (type1[0] == "register")
        {
            register_user(client_socket, type1[1], type1[2]);
        }
    }
    std::cout << "Connection closed" << endl;
    sqlite3_close(db);
    db = nullptr;
    close(client_socket);
}

void log_in(int client_socket, const string &username, const string &password) // Log in function
{
    
       sqlite3_stmt *stmt;
        string query = "SELECT username, password FROM Users WHERE username = ? AND password = ?";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
            return;
        }

        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW)
        {
            User user;
            user.username = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
            user.password = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
            std::cout << "Send: 220/" << user.username << endl;
            send(client_socket, "220/", strlen("220/"), 0);
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap[username] = client_socket;
            }
            sqlite3_finalize(stmt);
            functions(client_socket, user);
        }
        else
        {
            std::cout << "Send: 402/" << username << endl; 
            sqlite3_finalize(stmt);
            send(client_socket, "402/", strlen("402/"), 0);
        }

    }


void register_user(int client_socket, const string &username, const string &password)
{
    sqlite3_stmt *stmt;
    string query = "SELECT username FROM Users WHERE username = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return;
    }

    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        std::cout << "Send: 401/"<<username<< endl;
        sqlite3_finalize(stmt);
        send(client_socket, "401/", strlen("401/"), 0);
    }
    else
    {
        query = "INSERT INTO Users (username, password) VALUES (?, ?)";
        if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
        {
            cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);

            return;
        }
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) != SQLITE_DONE)
        {
            cerr << "Error inserting user data: " << sqlite3_errmsg(db) << endl;
            sqlite3_finalize(stmt);
        }
        User newUser;
        newUser.username = username;
        newUser.password = password;
        std::cout << "210/"<< newUser.username << endl;
        send(client_socket, "210/", strlen("210/"), 0);
        {
            std::lock_guard<std::mutex> lock(mapMutex);
            userSocketMap[username] = client_socket;
        }
        sqlite3_finalize(stmt);
        functions(client_socket, newUser);
    }
}
void search_flight1(int client_socket, const string &departure_point, const string &destination_point, const User &user)
{
    string noti = checknoti(client_socket);
    string msg;
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "311/";
  
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "411/" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        sqlite3_finalize(stmt);
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}
void search_flight2(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "311/";
    

    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}
void search_flight3(int client_socket, const string &departure_point, const string &destination_point, const string &return_date, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;
    bool found = false;

    string result_str = "311/";
    

    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND return_date >= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, return_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void search_flight4(int client_socket, const string &departure_point, const string &destination_point, const string &departure_date, const string &return_date, const User &user)
{
    string msg;
    string noti = checknoti(client_socket);
    sqlite3_stmt *stmt;

    bool found = false;

    string result_str = "311/";
   
    string query = "SELECT * FROM Flights WHERE departure_point = ? AND destination_point = ? AND departure_date <= ? AND return_date >= ?";

    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "N_found" + noti;
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    sqlite3_bind_text(stmt, 1, departure_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, destination_point.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, departure_date.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, return_date.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        found = true;
        Flights flight;
        flight.company = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        flight.flight_num = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        flight.num_A = sqlite3_column_int(stmt, 2);
        flight.num_B = sqlite3_column_int(stmt, 3);
        flight.price_A = sqlite3_column_int(stmt, 4);
        flight.price_B = sqlite3_column_int(stmt, 5);
        flight.departure_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6));
        flight.destination_point = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
        flight.departure_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 8));
        flight.return_date = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 9));

        result_str += flight.company + ",";
        result_str += flight.flight_num + ",";
        result_str += to_string(flight.num_A) + ",";
        result_str += to_string(flight.num_B) + ",";
        result_str += to_string(flight.price_A) + " VND" + ",";
        result_str += to_string(flight.price_B) + " VND" + ",";
        result_str += flight.departure_point + ",";
        result_str += flight.destination_point + ",";
        result_str += flight.departure_date + ",";
        result_str += flight.return_date + ";";
    }

    if (!found)
    {
        msg = "411/" + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
    else
    {
        msg = result_str + noti;
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }

    sqlite3_finalize(stmt);
}

void book_flight(int client_socket, const string flight_num, const string seat_class, const User &user){
    string msg;
    sqlite3_stmt *stmt;
    int ticket_price = 0;
    string query_price = "SELECT ";
    query_price += (seat_class == "A" ? "price_A" : "price_B");
    query_price += " FROM Flights WHERE flight_num = ?";

    sqlite3_prepare_v2(db, query_price.c_str(), -1, &stmt, nullptr);
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        ticket_price = sqlite3_column_int(stmt, 0);
    }
    else
    {
        cerr << "Failed to retrieve ticket price." << endl;
    }

    string query_seat;
    int available_seats;

    if (seat_class == "A")
    {
        query_seat = "SELECT seat_class_A FROM Flights WHERE flight_num = ?";
    }
    else if (seat_class == "B")
    {
        query_seat = "SELECT seat_class_B FROM Flights WHERE flight_num = ?";
    }
    else
    {
        msg = "431/";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }

    if (sqlite3_prepare_v2(db, query_seat.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        available_seats = sqlite3_column_int(stmt, 0);
    }
    else
    {
        msg = "432/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        sqlite3_finalize(stmt);
        return;
    }
    sqlite3_finalize(stmt);
    std::cout << "Seat available: " << available_seats << endl;
    if (available_seats == 0)
    {
        msg = "433/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    string query = "SELECT flight_num FROM Flights WHERE flight_num = ?";
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        msg = "434/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        cerr << "Error preparing query: " << sqlite3_errmsg(db) << endl;
        send(client_socket, msg.c_str(), msg.length(), 0);
        return;
    }
    sqlite3_bind_text(stmt, 1, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
    {
        sqlite3_finalize(stmt);

        int user_id = -1;
        string query1 = "SELECT user_id FROM Users WHERE username = ?";
        if (sqlite3_prepare_v2(db, query1.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, user.username.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                user_id = sqlite3_column_int(stmt, 0);
            }
            else
            {
                msg = "434/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
                return;
            }
            sqlite3_finalize(stmt);
        }
        else
        {
            cerr << "Error preparing user query: " << sqlite3_errmsg(db) << endl;
            msg = "434/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            send(client_socket, msg.c_str(), msg.length(), 0);
            sqlite3_finalize(stmt);
            return;
        }
        string ticket_code = generate_ticket_code();

        string payment_status = "NOT_PAID";
        string query2 = "INSERT INTO Tickets (ticket_code, user_id, flight_num, seat_class, ticket_price,payment) VALUES (?, ?, ?, ?, ?,?)";
        if (sqlite3_prepare_v2(db, query2.c_str(), -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text(stmt, 1, ticket_code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, user_id);
            sqlite3_bind_text(stmt, 3, flight_num.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, seat_class.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, ticket_price);
            sqlite3_bind_text(stmt, 6, payment_status.c_str(), -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_DONE)
            {
                std::cout << "Process booking and redirect to payment\n";
                string success_book = "330/";
                success_book += ticket_code + "/";
                success_book += to_string(ticket_price) + "/";
                msg = success_book;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);

                update_seat_count(db, flight_num, seat_class, 1);
                sqlite3_finalize(stmt);
            }
            else
            {
                msg = "434/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                cerr << "Error inserting ticket data: " << sqlite3_errmsg(db) << endl;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }
        else
        {
            msg = "434/";
            cout << "Send: " << msg << " ->" << user.username << "\n";
            cerr << "Error preparing insert query: " << sqlite3_errmsg(db) << endl;
            send(client_socket, msg.c_str(), msg.length(), 0);
        }
    }
    else
    {
        msg = "434/";
        cout << "Send: " << msg << " ->" << user.username << "\n";
        send(client_socket, msg.c_str(), msg.length(), 0);
    }
}

void update_seat_count(sqlite3 *db, const string &flight_num, const string &seat_class, int adjustment)
{
    sqlite3_stmt *stmt;
    string sql;
    if (seat_class == "A")
    {
        sql = "UPDATE Flights SET seat_class_A = seat_class_A - ? WHERE flight_num = ?";
    }
    else if (seat_class == "B")
    {
        sql = "UPDATE Flights SET seat_class_B = seat_class_B - ? WHERE flight_num = ?";
    }
    else
    {
        cerr << "Invalid seat class" << endl;
        return;
    }

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK)
    {
        cerr << "SQL error: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_int(stmt, 1, adjustment);
    sqlite3_bind_text(stmt, 2, flight_num.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_DONE)
    {
        cerr << "SQL error in updating seat count: " << sqlite3_errmsg(db) << endl;
    }

    sqlite3_finalize(stmt);
}


void functions(int client_socket, const User &user)
{
    char buffer[BUFFER_SIZE];
    int bytes_received;

    while (true)
    {
        bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';

        string received(buffer);
        cout << "Received: " << received << " from " << user.username << endl;
        if (received == "logout")
        {
            cout << "Send: 230/ ->" << user.username << "\n";
            send(client_socket, "230/", strlen("230/"), 0);
            {
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap.erase(user.username);
            }
            return;
        }

        if (received == "exit search request")
        {
            string msg;
            msg += "Exit search";
            send(client_socket, msg.c_str(), msg.length(), 0);
        }
       
        vector<string> type1 = split(received, '/');

        if (lower(type1[0]) == "search1")
        {
            search_flight1(client_socket, type1[1], type1[2], user);
        }
        if (lower(type1[0]) == "search2")
        {
            search_flight2(client_socket, type1[1], type1[2], type1[3], user);
        }
        if (lower(type1[0]) == "search3")
        {
            search_flight3(client_socket, type1[1], type1[2], type1[3], user);
        }
        if (lower(type1[0]) == "search4")
        {
            search_flight4(client_socket, type1[1], type1[2], type1[3], type1[4], user);
        }

        else if (lower(type1[0]) == "book")
        {
            book_flight(client_socket, type1[1], type1[2], user);
        }

        else if (lower(type1[0]) == "pay")
        {
            string msg;
            int ticket_price;
            sqlite3_stmt *stmt;
            string check = "SELECT ticket_price FROM Tickets WHERE ticket_code = ?";

            sqlite3_prepare_v2(db, check.c_str(), -1, &stmt, NULL);
            sqlite3_bind_text(stmt, 1, type1[1].c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) == SQLITE_ROW)
            {
                ticket_price = sqlite3_column_int(stmt, 0);
            }

            sqlite3_finalize(stmt);

            if ((type1[3] == "card" && type1[4].length() != 16) ||
                (type1[3] == "e-wallet" && type1[4].length() != 10)) {
                string response = "442/"; // Invalid format
                send(client_socket, response.c_str(), response.length(), 0);
                return;
            }

            string update_pay = "UPDATE Tickets SET payment = 'PAID' WHERE ticket_code = ?";

            if (sqlite3_prepare_v2(db, update_pay.c_str(), -1, &stmt, NULL) != SQLITE_OK)
            {
                msg = "443/";
                cerr << "Error preparing update statement" << endl;
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else
            {
                if (sqlite3_bind_text(stmt, 1, type1[1].c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK)
                {
                    msg = "443/";
                    cerr << "Error binding ticket_code to update statement" << endl;
                    cout << "Send: " << msg << " ->" << user.username << "\n";
                    send(client_socket, msg.c_str(), msg.length(), 0);
                }
                else
                {
                    if (sqlite3_step(stmt) != SQLITE_DONE)
                    {
                        msg = "443/";
                        cerr << "Error executing update statement" << endl;
                        cout << "Send: " << msg << " ->" << user.username << "\n";
                        send(client_socket, msg.c_str(), msg.length(), 0);
                    }
                }
                sqlite3_finalize(stmt);
                msg = "341/";
                cout << "Send: " << msg << " ->" << user.username << "\n";
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
        }

   
}
}
