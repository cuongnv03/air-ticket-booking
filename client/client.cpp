#include <iostream>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 54000
#define BUFFER_SIZE 1024

void print_main_menu() {
    std::cout << "1. Login\n2. Register\n3. Logout\nYour choice: ";
}

void print_functions() {
    std::cout << "__________________________________________________\n";
    std::cout << "1. Search Flights\n2. Book tickets\n3. View tickets detail\n4. Cancel tickets\n5. Change tickets\n6. Print tickets\n7. Ticket payment\n8. Log out" << std::endl;
    std::cout << "__________________________________________________\n";
    std::cout << "Your message: ";
}

void handle_login(int client_socket) {
    std::string username, password;
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    std::string message = "LOGIN/" + username + "/" + password;
    send(client_socket, message.c_str(), message.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes_received] = '\0';

    std::string response(buffer);
    std::istringstream iss(response);
    std::string code, user;
    std::getline(iss, code, '/');
    std::getline(iss, user);

    if (code == "220") {
        std::cout << user << " logged in successfully." << std::endl;
        while (true) {
            print_functions();
            int choice;
            std::cin >> choice;
            if (choice == 8) {
                handle_logout(client_socket);
                break;
            }
            // Handle other menu options here
        }
    } else if (code == "402") {
        std::cout << "User " << user << " has not existed." << std::endl;
    } else if (code == "403") {
        std::cout << "Invalid password for " << user << "." << std::endl;
    }
}

void handle_register(int client_socket) {
    std::string username, password;
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;

    std::string message = "REGISTER/" + username + "/" + password;
    send(client_socket, message.c_str(), message.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes_received] = '\0';

    std::string response(buffer);
    std::istringstream iss(response);
    std::string code, user;
    std::getline(iss, code, '/');
    std::getline(iss, user);

    if (code == "210") {
        std::cout << user << " registered successfully and logged in." << std::endl;
        while (true) {
            print_functions();
            int choice;
            std::cin >> choice;
            if (choice == 8) {
                handle_logout(client_socket);
                break;
            }
            // Handle other menu options here
        }
    } else if (code == "401") {
        std::cout << "User " << user << " has been used." << std::endl;
    }
}

void handle_logout(int client_socket) {
    std::string username;
    std::cout << "Enter username: ";
    std::cin >> username;

    std::string message = "LOGOUT/";
    send(client_socket, message.c_str(), message.size(), 0);

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    buffer[bytes_received] = '\0';

    std::string response(buffer);
    if (response == "230") {
        std::cout << "You’ve logged out successfully." << std::endl;
    }
}

int main() {
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Error creating socket.\n";
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(client_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Error connecting to server.\n";
        close(client_socket);
        return -1;
    }

    char buffer[BUFFER_SIZE];
    while (true) {
        print_main_menu();
        int choice;
        std::cin >> choice;

        switch (choice) {
            case 1:
                handle_login(client_socket);
                break;
            case 2:
                handle_register(client_socket);
                break;
            case 3:
                handle_logout(client_socket);
                close(client_socket);
                return 0;
            default:
                std::cout << "Invalid choice. Please try again.\n";
        }
    }

    close(client_socket);
    return 0;
}