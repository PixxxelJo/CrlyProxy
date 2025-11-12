// LANServer.h
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#endif

#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <atomic>

class LANServer {
public:
    LANServer(unsigned short port) 
        : port_(port), running_(false) {}

    bool start() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }
#endif

        serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket_ < 0) {
            perror("socket failed");
            return false;
        }

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port_);

        if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            perror("bind failed");
            return false;
        }

        if (listen(serverSocket_, 5) < 0) {
            perror("listen failed");
            return false;
        }

        running_ = true;
        acceptThread_ = std::thread(&LANServer::acceptClients, this);
        std::cout << "Master server started on port " << port_ << "\n";
        return true;
    }

    void stop() {
        running_ = false;
#ifdef _WIN32
        closesocket(serverSocket_);
        WSACleanup();
#else
        close(serverSocket_);
#endif
        if (acceptThread_.joinable()) acceptThread_.join();
        std::lock_guard<std::mutex> lock(clientMutex_);
        for (auto s : clients_) {
#ifdef _WIN32
            closesocket(s);
#else
            close(s);
#endif
        }
        clients_.clear();
        std::cout << "Master server stopped\n";
    }

    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(clientMutex_);
        for (auto s : clients_) {
            send(s, message.c_str(), message.size(), 0);
        }
    }

private:
    void acceptClients() {
        while (running_) {
            sockaddr_in clientAddr{};
#ifdef _WIN32
            int addrLen = sizeof(clientAddr);
#else
            socklen_t addrLen = sizeof(clientAddr);
#endif
            int clientSocket = accept(serverSocket_, (struct sockaddr*)&clientAddr, &addrLen);
            if (clientSocket < 0) continue;

            {
                std::lock_guard<std::mutex> lock(clientMutex_);
                clients_.push_back(clientSocket);
            }

            std::thread(&LANServer::handleClient, this, clientSocket).detach();
            std::cout << "Client connected: " << inet_ntoa(clientAddr.sin_addr) << "\n";
        }
    }

    void handleClient(int clientSocket) {
        char buffer[1024];
        while (running_) {
            int bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) break;
            buffer[bytesRead] = '\0';
            std::string msg = buffer;
            std::cout << "[Client]: " << msg;
            broadcast("[Client]: " + msg);
        }

        {
            std::lock_guard<std::mutex> lock(clientMutex_);
            clients_.erase(std::remove(clients_.begin(), clients_.end(), clientSocket), clients_.end());
        }
#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
        std::cout << "Client disconnected\n";
    }

    unsigned short port_;
    std::atomic<bool> running_;
    int serverSocket_;
    std::thread acceptThread_;
    std::mutex clientMutex_;
    std::vector<int> clients_;
};
