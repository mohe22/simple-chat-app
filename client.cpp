#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <ctime>
#include <chrono>

const int PORT = 8080;
const char* IP = "127.0.0.1";
const int BUFFER_SIZE = 4096;

std::atomic<bool> running(true);

int createSocket();
bool setSocketOptions(int sock);
bool connectToServer(int sock, const sockaddr_in& serv_addr);
void registerClient(int sock, const std::string& name);
void handleReceivedMessage(const nlohmann::json& json_data);
void receiveMessages(int sock);
void sendMessage(int sock, const std::string& name, const std::string& message);
void getUserInput(int sock, const std::string& name);

int main() {
    // Create and configure socket
    int sock = createSocket();
    if (sock < 0) {
        return -1;
    }

    // Set socket options
    if (!setSocketOptions(sock)) {
        close(sock);
        return -1;
    }

    // Configure server address
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/Address not supported\n";
        close(sock);
        return -1;
    }

    // Connect to server
    if (!connectToServer(sock, serv_addr)) {
        close(sock);
        return -1;
    }

    // Get and register user name
    std::cout << "Enter your name: ";
    std::string name;
    std::getline(std::cin, name);
    registerClient(sock, name);

    // Start message receiving thread
    std::thread receiveThread(receiveMessages, sock);

    // Handle user input in main thread
    getUserInput(sock, name);

    // Cleanup
    receiveThread.join();
    close(sock);
    return 0;
}

int createSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation error\n";
    }
    return sock;
}

bool setSocketOptions(int sock) {
    // Set socket reuse option
    int opt = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("[-] setsockopt failed");
        return false;
    }

    // Set non-blocking mode
    if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
        perror("[-] Failed to set non-blocking mode");
        return false;
    }

    return true;
}

bool connectToServer(int sock, const sockaddr_in& serv_addr) {
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) {
        if (errno != EINPROGRESS) {
            perror("connection error");
            return false;
        }
    }
    return true;
}

void registerClient(int sock, const std::string& name) {
    nlohmann::json json_data;
    json_data["name"] = name;
    json_data["type"] = "register";

    std::string json_string = json_data.dump();
    if (!json_string.empty()) {
        send(sock, json_string.c_str(), json_string.length(), 0);
        std::cout << "Your name has been sent to the server.\n";
    }
}

void handleReceivedMessage(const nlohmann::json& json_data) {
    if (json_data.contains("type")) {
        std::string type = json_data["type"];
        std::string name = json_data["name"];

        std::cout << std::endl;
        if (type == "join") {
            std::cout << name << " joined the chat!\n";
        } else {
            std::cout << name << " left the chat!\n";
        }
    } else if (json_data.contains("message")) {
        std::string message = json_data["message"];
        std::string name = json_data["name"];
        std::string time = json_data["time"];

        std::cout << std::endl;
        std::cout << "[" << time << "," << name << "] " << message << "\n";
    }
    std::cout << "Send Message: ";
    std::cout.flush();
}

void receiveMessages(int sock) {
    char buffer[BUFFER_SIZE];
    while (running) {
        ssize_t valread = read(sock, buffer, BUFFER_SIZE);
        if (valread > 0) {
            buffer[valread] = '\0';
            try {
                nlohmann::json json_data = nlohmann::json::parse(buffer);
                handleReceivedMessage(json_data);
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
            }
        } else if (valread == 0) {
            std::cout << "Server closed the connection.\n";
            running = false;
            break;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Read error");
                running = false;
                break;
            }
        }
    }
}

void sendMessage(int sock, const std::string& name, const std::string& message) {
    nlohmann::json json_data;
    json_data["name"] = name;
    json_data["message"] = message;

    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    char time_str[6];
    strftime(time_str, sizeof(time_str), "%H:%M", std::localtime(&now_time));
    json_data["time"] = time_str;

    std::string json_string = json_data.dump();
    send(sock, json_string.c_str(), json_string.length(), 0);
}

void getUserInput(int sock, const std::string& name) {
    while (running) {
        std::cout << "Send Message: ";
        std::string msg;
        std::getline(std::cin, msg);
        
        if (!msg.empty()) {
            sendMessage(sock, name, msg);
        }

        usleep(100000);  // Small delay to prevent CPU overuse
    }
}