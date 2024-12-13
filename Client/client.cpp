#include "client.h"

int main()
{
    std::string tmp_noti;
    std::cout << "Enter IP: ";

    std::string chost;
    getline(cin, chost);
    const char *host = chost.c_str();
    struct sockaddr_in server_addr;
    int client_socket;
    Role cur_role = Role::none;

    try
    {
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1)
        {
            throw runtime_error("Error creating client socket");
        }

        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(SERV_PORT);
        if (inet_pton(AF_INET, host, &(server_addr.sin_addr)) <= 0)
        {
            cerr << "Invalid IP address" << endl;
            return 1;
        }

        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        {
            throw runtime_error("Error connecting to the server");
        }

        char buffer[BUFFER_SIZE];

        while (true)
        {
            print_main_menu();
            string choice;
            getline(cin, choice);
            string lower_choice = trim(lower(choice));
            if (lower_choice == "3")
            {
                send(client_socket, lower_choice.c_str(), choice.length(), 0);
                break;
            }
            else if (lower_choice == "1")
            {
                string username, password;
                cout << "Enter username: ";
                getline(cin, username);
                cout << "Enter password: ";
                getline(cin, password);
                string msg = "login/" + username + "/" + password;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else if (lower_choice == "2")
            {
                string username, password;
                cout << "Enter new username: ";
                getline(cin, username);
                cout << "Enter new password: ";
                getline(cin, password);
                string msg = "register/" + username + "/" + password;
                send(client_socket, msg.c_str(), msg.length(), 0);
            }
            else
            {
                std::cout << "Invalid choice!\n";
                continue;
            }
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0)
            {
                break;
            }
            buffer[bytes_received] = '\0';

            string message(buffer);
            string response = string(buffer);

            if (response == "210/" || response == "220/")
            {
                cout << "You're currently online!" << endl;
                cur_role = Role::user;
                while (true)
                {
                    if (tmp_noti.length() != 0)
                    {
                        cout << "Notification: \n";
                        cout << tmp_noti;
                        tmp_noti = "";
                    }
                    print_functions();

                    string choice1;
                    getline(cin, choice1);
                    string lower_choice1 = trim(lower(choice1));
                    // if (lower_choice1 == "exit")
                    // {
                    //     send(client_socket, lower_choice.c_str(), choice.length(), 0);
                    //     break;
                    // }
                    //search
                    if (lower_choice1 == "1")
                    {
                        string company, destination_point, departure_point, departure_date, return_date;
                        string search_msg;
                        while (1)
                        {
                            print_menu_search();
                            string choice2;
                            getline(cin, choice2);
                            
                            // cin >> choice2;
                            // cin.ignore(numeric_limits<streamsize>::max(), '\n');
                        
                            if (choice2 == "1")
                            {
                                cout << "Enter departure point: ";
                                getline(cin, departure_point);
                                cout << "Enter destination point: ";
                                getline(cin, destination_point);
                                search_msg += "search1/" + departure_point + "/" + destination_point;
                                break;
                            }
                            else if (choice2 == "3")
                            {
                                
                                
                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);
                                cout << "Enter return date : ";
                                getline(cin, return_date);
                                search_msg += "search3/"  + departure_point + "/" + destination_point+ "/" + return_date;
                                break;
                            }
                            else if (choice2 == "2")
                            {
                                
                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);

                                cout << "Enter departure date (or leave blank for any, format YYYY-MM-DD): ";
                                getline(cin, departure_date);
                                search_msg += "search2/" + departure_point + "/" + destination_point + "/" + departure_date;
                                break;
                            }
                            else if (choice2 == "4")
                            {
                               
                                cout << "Enter departure point : ";
                                getline(cin, departure_point);

                                cout << "Enter destination point : ";
                                getline(cin, destination_point);

                                cout << "Enter departure date (or leave blank for any, format YYYY-MM-DD): ";
                                getline(cin, departure_date);

                                cout << "Enter return date (or leave blank for any, format YYYY-MM-DD): ";
                                getline(cin, return_date);
                                search_msg += "search4/" + departure_point + "/" + destination_point + "/" + departure_date + "/" + return_date;
                                break;
                            }
                            else if (choice2 == "5")
                            {
                                printf("Exit search\n");
                                send(client_socket, "exit search request", strlen("exit search request"), 0);
                                break;
                            }
                            else
                            {
                                std::cout << "Invalid choice!\n";
                            }
    
                        }
                        send(client_socket, search_msg.c_str(), search_msg.length(), 0);
                    }

                    else if (lower_choice1 == "2"){
                        string flight_num, seat_class;
                        cout << "Enter your flight ID: ";
                        getline(cin, flight_num);
                        cout << "Enter your desired seat class (A/B): ";
                        getline(cin, seat_class);
                        string book_msg = "book/" + flight_num + "/" + seat_class;
                        send(client_socket, book_msg.c_str(), book_msg.length(), 0);

                        memset(buffer, 0, BUFFER_SIZE);
                        recv(client_socket, buffer, BUFFER_SIZE, 0);
                        string response(buffer);
                        istringstream iss(response);
                        string code, ticket_code, ticket_price;
                        getline(iss, code, '/');
                        if (code == "330") {
                            getline(iss, ticket_code, '/');
                            getline(iss, ticket_price, '/');
                            cout << "You are booking flight ticket with ticket ID: " << ticket_code
                                << " and you need to pay " << ticket_price << "đ to complete booking.\n";
                            
                            // Automatically redirect to pay method
                            string pay_method, pay_details;
                            cout << "Enter payment method (card/e-wallet): ";
                            cin >> pay_method;

                            if (pay_method != "card" && pay_method != "e-wallet") {
                                cout << "Invalid payment method. Choose 'card' or 'e-wallet'.\n";
                                continue;
                            }
                            cout << "Enter payment details: ";
                            cin >> pay_details;

                            if ((pay_method == "card" && pay_details.length() != 16) ||
                                (pay_method == "e-wallet" && pay_details.length() != 10)) {
                                cout << "Invalid payment details format.\n";
                                continue;
                            }
                            string pay_command = "pay/" + ticket_code + "/" + ticket_price + "/" + pay_method + "/" + pay_details;
                            send(client_socket, pay_command.c_str(), pay_command.length(), 0);

                            memset(buffer, 0, BUFFER_SIZE);
                            recv(client_socket, buffer, BUFFER_SIZE, 0);
                            string response(buffer);
                            istringstream iss(response);
                            string pay_code;
                            getline(iss, pay_code, '/');
                            if (pay_code == "341" || pay_code == "342") {
                                cout << "You have paid " << ticket_price << " successfully with " << pay_method << " " << pay_details
                                    << ". Your ticket ID is " << ticket_code << ".\n";
                                continue;
                            } else if (pay_code == "441") {
                                cout << "You don't have enough money to pay this.\n";
                                continue;
                            } else if (pay_code == "442") {
                                cout << "Payment detail does not exist.\n";
                                continue;
                            } else if (pay_code == "443") {
                                cout << "Invalid format of input.\n";
                                continue;
                            } else {
                                cout << "Unexpected server response: " << response << endl;
                                continue;
                            }
                            continue;
                        } else if (code == "431") {
                            cout << "Wrong seat class in this flight. Only choose A or B.\n";
                            continue;
                        } else if (code == "432") {
                            cout << "There is no flight with that flight ID.\n";
                            continue;
                        } else if (code == "433") {
                            cout << "There is no seat left of that seat class.\n";
                            continue;
                        } else if (code == "434") {
                            cout << "Error booking flight ticket.\n";
                            continue;
                        } else {
                            cout << "Unexpected server response: " << response << endl;
                            continue;
                        }
                        
                    }
 
                    else if (lower_choice1 == "8")
                    {
                        send(client_socket, "logout", strlen("logout"), 0);
                        memset(buffer, 0, BUFFER_SIZE);
                        int bytes_received1 = recv(client_socket, buffer, BUFFER_SIZE, 0);
                        if (bytes_received1 <= 0)
                        {
                            break;
                        }
                        buffer[bytes_received1] = '\0';

                        string response1 = string(buffer);
                        if (response1 == "230/")
                        {
                            cur_role = Role::none; // Resetting the role to none
                            std::cout << "You've logged out successfully!" << endl;
                            break;
                        }
                    }
                    else
                    {
                        std::cout << "Invalid choice\n";
                        continue;
                    }
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes_received1 = recv(client_socket, buffer, BUFFER_SIZE, 0);
                    if (bytes_received1 <= 0)
                    {
                        break;
                    }
                    buffer[bytes_received1] = '\0';

                    string response1 = string(buffer);


                    if (response1.find("311/") == 0)
                    {
                        string flight_data = response1.substr(8);
                        std::cout << "Flight data:" << endl;
                        display_search(flight_data);
                    }
                    else if (response1.find("411/") == 0)
                    {
                        std::cout << "Can't find the flight!\n";
                    }
                }
            }
            else if (response == "402/")
            {
                cout << "Login failed. Please check your username and password." << endl;
            }
           
            else if (response == "401/")
            {
                std::cout << "Your username has already existed!" << endl;
            }

        }

        close(client_socket);
        std::cout << "Closed the connection." << endl;
    }
    catch (const exception &e)
    {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }
    return 0;
}
