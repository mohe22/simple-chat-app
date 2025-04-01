#include <iostream>     
#include <cstring>     
#include <cstdlib>              // For general utilities (like memory allocation, exit)
#include <unistd.h>             // For system calls (e.g., read(), write(), close())
#include <errno.h>              // For error handling (e.g., errno)
#include <arpa/inet.h>          // For IP address manipulation (e.g., inet_pton())
#include <sys/socket.h>         // For socket programming (e.g., socket(), bind())
#include <sys/epoll.h>          // For efficient event notification (e.g., epoll)
#include <fcntl.h>              // For file control operations (e.g., fcntl())
#include <nlohmann/json.hpp>    // for json

using namespace std;

// Constants
const int PORT = 8080;
const int MAX_EVENTS = 20;
const int BUFF_SIZE = 1024;


struct Client {
    string name;
    int fd;
};

// Function declarations
int setNonBlocking(int fd);
int setupServerSocket();
int setupEpoll(int serverFd);
void handleNewConnection(int epollFd, int serverFd);
void handleClientMessage(int epollFd, int clientFd, vector<Client>& clients);
void registerClient(int clientFd, const string& name, vector<Client>& clients);
void removeClient(int clientFd, vector<Client>& clients);
void broadcastMessage(const vector<Client>& clients, const nlohmann::json& message, int senderFd);

int main() {
    vector<Client> clients;
    
    // Setup server socket
    int serverFd = setupServerSocket();
    if (serverFd == -1) {
        return EXIT_FAILURE;
    }
    
    // Setup epoll
    int epollFd = setupEpoll(serverFd);
    if (epollFd == -1) {
        close(serverFd);
        return EXIT_FAILURE;
    }
    
    cout << "Server is listening on port " << PORT << endl;

    struct epoll_event events[MAX_EVENTS];
    while (true) {
        int numEvents = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if (numEvents == -1) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < numEvents; i++) {
            if (events[i].data.fd == serverFd) {
                handleNewConnection(epollFd, serverFd);
            } else {
                handleClientMessage(epollFd, events[i].data.fd, clients);
            }
        }
    }

    close(serverFd);
    close(epollFd);
    return 0;
}

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl(F_GETFL) error");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl(F_SETFL) error");
        return -1;
    }
    return 0;
}

int setupServerSocket() {
    // Create socket
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd == -1) {
        perror("socket");
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(serverFd);
        return -1;
    }

    // Set non-blocking
    if (setNonBlocking(serverFd) == -1) {
        close(serverFd);
        return -1;
    }

    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(serverFd);
        return -1;
    }

    // Listen
    if (listen(serverFd, 10) == -1) {
        perror("listen");
        close(serverFd);
        return -1;
    }

    return serverFd;
}

int setupEpoll(int serverFd) {
    int epollFd = epoll_create1(0);
    if (epollFd == -1) {
        perror("epoll_create1");
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = serverFd;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, serverFd, &event) == -1) {
        perror("epoll_ctl: serverFd");
        close(epollFd);
        return -1;
    }

    return epollFd;
}

void handleNewConnection(int epollFd, int serverFd) {
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    
    while (true) {
        int clientFd = accept(serverFd, (struct sockaddr*)&address, &addrlen);
        if (clientFd == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("accept");
            }
            break;
        }

        if (setNonBlocking(clientFd) == -1) {
            close(clientFd);
            continue;
        }

        struct epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = clientFd;
        if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &event) == -1) {
            perror("epoll_ctl: clientFd");
            close(clientFd);
        }
    }
}

void handleClientMessage(int epollFd, int clientFd, vector<Client>& clients) {
    char buffer[BUFF_SIZE];
    
    while (true) {
        ssize_t count = read(clientFd, buffer, BUFF_SIZE);
        if (count == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            perror("read");
            removeClient(clientFd, clients);
            close(clientFd);
            break;
        } else if (count == 0) {
            removeClient(clientFd, clients);
            close(clientFd);
            break;
        }

        buffer[count] = '\0';
        try {
            nlohmann::json json_data = nlohmann::json::parse(string(buffer, count));
            
            if (json_data.contains("type") && json_data["type"] == "register") {
                registerClient(clientFd, json_data["name"], clients);
            } else {
                broadcastMessage(clients, json_data, clientFd);
            }
        } catch (const nlohmann::json::parse_error& e) {
            cerr << "Error parsing JSON: " << e.what() << endl;
        }
    }
}

void registerClient(int clientFd, const string& name, vector<Client>& clients) {
    Client newClient;
    newClient.name = name;
    newClient.fd = clientFd;
    clients.push_back(newClient);
    
    nlohmann::json joinMessage;
    joinMessage["name"] = name;
    joinMessage["type"] = "join";
    broadcastMessage(clients, joinMessage, clientFd);
}

void removeClient(int clientFd, vector<Client>& clients) {
    auto it = find_if(clients.begin(), clients.end(), 
        [clientFd](const Client& client) { return client.fd == clientFd; });
    
    if (it != clients.end()) {
        cout << "User " << it->name << " disconnected." << endl;
        
        nlohmann::json disconnectMessage;
        disconnectMessage["name"] = it->name;
        disconnectMessage["type"] = "disconnected";
        broadcastMessage(clients, disconnectMessage, -1);
        
        clients.erase(it);
    }
}

void broadcastMessage(const vector<Client>& clients, const nlohmann::json& message, int senderFd) {
    string json_string = message.dump();
    for (const auto& client : clients) {
        if (client.fd != senderFd) {
            ssize_t writeResult = write(client.fd, json_string.c_str(), json_string.length());
            if (writeResult == -1) {
                perror("Error sending message to client");
            }
        }
    }
}