#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <vector>
#include <windows.h>
#include <sstream>
#include <thread>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

enum ConsoleColor {
    BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
    YELLOW = 6, WHITE = 7, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10
};

struct ManagerConfig {
    const char* ip;
    int port;
};

const ManagerConfig TEXT_MANAGER = { "127.0.0.1", 9091 };
const ManagerConfig IMAGE_MANAGER = { "127.0.0.1", 9092 };
const ManagerConfig ARRAY_MANAGER = { "127.0.0.1", 9093 };

void setColor(ConsoleColor color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void printHeader(const std::string& text) {
    setColor(BRIGHT_BLUE);
    std::cout << "\n=== " << text << " ===\n";
    setColor(WHITE);
}

void printSuccess(const std::string& message) {
    setColor(BRIGHT_GREEN);
    std::cout << "[SUCCESS] " << message << std::endl;
    setColor(WHITE);
}

void printError(const std::string& message) {
    setColor(RED);
    std::cerr << "[ERROR] " << message << std::endl;
    setColor(WHITE);
}

bool sendAll(SOCKET s, const char* data, int size) {
    int totalSent = 0;
    while (totalSent < size) {
        int sent = send(s, data + totalSent, size - totalSent, 0);
        if (sent == SOCKET_ERROR) return false;
        totalSent += sent;
    }
    return true;
}

bool receiveAll(SOCKET s, char* buffer, int size) {
    int totalReceived = 0;
    while (totalReceived < size) {
        int received = recv(s, buffer + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

void handleClient(SOCKET clientSocket) {
    try {
        uint32_t jobTypeLen;
        if (!receiveAll(clientSocket, (char*)&jobTypeLen, sizeof(jobTypeLen))) {
            printError("Job type length receive failed");
            closesocket(clientSocket);
            return;
        }
        jobTypeLen = ntohl(jobTypeLen);

        std::vector<char> jobTypeBuffer(jobTypeLen);
        if (!receiveAll(clientSocket, jobTypeBuffer.data(), jobTypeLen)) {
            printError("Job type receive failed");
            closesocket(clientSocket);
            return;
        }
        std::string jobType(jobTypeBuffer.begin(), jobTypeBuffer.end());

        ManagerConfig targetManager;
        if (jobType.find("text_") == 0) {
            targetManager = TEXT_MANAGER;
            printHeader("Routing to Text Manager");
        }
        else if (jobType.find("image_") == 0) {
            targetManager = IMAGE_MANAGER;
            printHeader("Routing to Image Manager");
        }
        else if (jobType.find("array_") == 0) {
            targetManager = ARRAY_MANAGER;
            printHeader("Routing to Array Manager");
        }
        else {
            printError("Unsupported job type: " + jobType);
            const char* errorMsg = "Invalid job type";
            sendAll(clientSocket, errorMsg, strlen(errorMsg));
            closesocket(clientSocket);
            return;
        }

        uint32_t dataSize;
        if (!receiveAll(clientSocket, (char*)&dataSize, sizeof(dataSize))) {
            printError("Data size receive failed");
            closesocket(clientSocket);
            return;
        }
        dataSize = ntohl(dataSize);

        SOCKET managerSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (managerSocket == INVALID_SOCKET) {
            printError("Failed to create manager socket");
            closesocket(clientSocket);
            return;
        }

        sockaddr_in managerAddr{};
        managerAddr.sin_family = AF_INET;
        inet_pton(AF_INET, targetManager.ip, &managerAddr.sin_addr);
        managerAddr.sin_port = htons(targetManager.port);

        if (connect(managerSocket, (sockaddr*)&managerAddr, sizeof(managerAddr)) == SOCKET_ERROR) {
            printError("Failed to connect to manager: " + std::string(targetManager.ip) + ":" + std::to_string(targetManager.port));
            closesocket(managerSocket);
            closesocket(clientSocket);
            return;
        }

        uint32_t typeLengthNet = htonl(jobType.size());
        if (!sendAll(managerSocket, reinterpret_cast<char*>(&typeLengthNet), sizeof(typeLengthNet)) ||
            !sendAll(managerSocket, jobType.c_str(), jobType.size())) {
            printError("Failed to send job type to manager");
            closesocket(managerSocket);
            closesocket(clientSocket);
            return;
        }

        uint32_t dataSizeNet = htonl(dataSize);
        if (!sendAll(managerSocket, reinterpret_cast<char*>(&dataSizeNet), sizeof(dataSizeNet))) {
            printError("Failed to send data size to manager");
            closesocket(managerSocket);
            closesocket(clientSocket);
            return;
        }

        const size_t CHUNK_SIZE = 4096;
        std::vector<char> buffer(CHUNK_SIZE);
        uint32_t totalReceived = 0;
        bool transferError = false;

        while (totalReceived < dataSize && !transferError) {
            uint32_t remaining = dataSize - totalReceived;
            uint32_t chunkSize = static_cast<uint32_t>(std::min<size_t>(remaining, CHUNK_SIZE));
            int bytesReceived = recv(clientSocket, buffer.data(), chunkSize, 0);
            if (bytesReceived <= 0) {
                printError("Failed to receive data from client");
                transferError = true;
                break;
            }

            if (!sendAll(managerSocket, buffer.data(), bytesReceived)) {
                printError("Failed to send chunk to manager");
                transferError = true;
                break;
            }
            totalReceived += bytesReceived;
        }

        if (transferError) {
            closesocket(managerSocket);
            closesocket(clientSocket);
            return;
        }

        uint32_t responseSize;
        if (!receiveAll(managerSocket, reinterpret_cast<char*>(&responseSize), sizeof(responseSize))) {
            printError("Failed to receive response size from manager");
            closesocket(managerSocket);
            closesocket(clientSocket);
            return;
        }
        responseSize = ntohl(responseSize);

        std::vector<char> response;
        if (responseSize > 0) {
            response.resize(responseSize);
            if (!receiveAll(managerSocket, response.data(), responseSize)) {
                printError("Failed to receive response data from manager");
                response.clear();
            }
        }

        if (response.empty()) {
            const char* errorMsg = "Empty response from manager";
            sendAll(clientSocket, errorMsg, strlen(errorMsg));
            printError(errorMsg);
        }
        else {
            uint32_t responseSizeNet = htonl(response.size());
            if (!sendAll(clientSocket, reinterpret_cast<char*>(&responseSizeNet), sizeof(responseSizeNet)) ||
                !sendAll(clientSocket, response.data(), response.size())) {
                printError("Failed to send response to client");
            }
            else {
                printSuccess("Job completed successfully");
            }
        }

        closesocket(managerSocket);
    }
    catch (const std::exception& e) {
        printError("Exception in client handling: " + std::string(e.what()));
    }
    catch (...) {
        printError("Unknown exception in client handling");
    }
    closesocket(clientSocket);
}

int main() {
    WSADATA wsa;
    SOCKET serverSocket;
    sockaddr_in serverAddr{};

    setColor(CYAN);
    std::cout << "Initializing Server...\n";

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printError("WSAStartup failed");
        return 1;
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        printError("Socket creation failed");
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printError("Bind failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        printError("Listen failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    printSuccess("Server listening on port 8080");

    while (true) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET) {
            std::thread([clientSocket]() {
                handleClient(clientSocket);
                }).detach();
        }
        else {
            printError("Accept failed");
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}