#include "LANServer.h"
#include <csignal>
#include <atomic>
#include <iostream>

std::atomic<bool> running{true};

void signalHandler(int) {
    running = false;
}

int main() {
    LANServer server(5000);

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    server.start();

    while (running) {
        std::string input;
        std::getline(std::cin, input);
        if (!input.empty()) {
            server.broadcast("[SERVER]: " + input);
        }
    }

    server.stop();
    return 0;
}
