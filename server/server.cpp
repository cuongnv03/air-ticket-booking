#include <iostream>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <mutex>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 54000
#define BUFFER_SIZE 1024

std::unordered_map<std::string, int> userSocketMap;
std::mutex mapMutex;

bool check_credentials(const std::string& username, const std::string& password) {
    std::ifstream file("users.txt");
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string user, pass;
        std::getline(iss, user, ':');
        std::getline(iss, pass);
        if (user == username) {
            if (pass == password) {
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

bool user_exists(const std::string& username) {
    std::ifstream file("users.txt");
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string user;
        std::getline(iss, user, ':');
        if (user == username) return true;
    }
    return false;
}

bool register_user(const std::string& username, const std::string& password) {
    if (user_exists(username)) return false;
    std::ofstream outfile;
    outfile.open("users.txt", std::ios_base::app);
    outfile << username << ":" << password << "\n";
    return true;
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) break;

        buffer[bytes_received] = '\0';
        std::string request(buffer);
        std::istringstream iss(request);
        std::string command, username, password;
        std::getline(iss, command, '/');
        std::getline(iss, username, '/');
        std::getline(iss, password);

        std::string response;
        if (command == "LOGIN") {
            if (user_exists(username)) {
                if (check_credentials(username, password)) {
                    response = "220/" + username;
                    std::lock_guard<std::mutex> lock(mapMutex);
                    userSocketMap[username] = client_socket;
                } else {
                    response = "403/" + username;
                }
            } else {
                response = "402/" + username;
            }
        } else if (command == "REGISTER") {
            if (register_user(username, password)) {
                response = "210/" + username;
                std::lock_guard<std::mutex> lock(mapMutex);
                userSocketMap[username] = client_socket;
            } else {
                response = "401/" + username;
            }
        } else if (command == "LOGOUT") {
            std::lock_guard<std::mutex> lock(mapMutex);
            userSocketMap.erase(username);
            response = "230";
            send(client_socket, response.c_str(), response.size(), 0);
            break;
        } else {
            response = "Unknown command!";
        }

        send(client_socket, response.c_str(), response.size(), 0);
    }
    close(client_socket);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        std::cerr << "Error creating socket.\n";
        return -1;
    }

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Error binding to port.\n";
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, SOMAXCONN) == -1) {
        std::cerr << "Error listening on socket.\n";
        close(server_socket);
        return -1;
    }

    while (true) {
        sockaddr_in client_addr;
        socklen_t client_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_size);
        if (client_socket == -1) {
            std::cerr << "Error accepting connection.\n";
            continue;
        }

        std::thread client_thread(handle_client, client_socket);
        client_thread.detach();
    }

    close(server_socket);
    return 0;
}