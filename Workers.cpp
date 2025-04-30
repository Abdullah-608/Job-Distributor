#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <thread>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

// Configuration
const string MANAGER_IP = "127.0.0.1";
const int MANAGER_PORT = 9090;
const int WORKER_PORT = 10001;
const int MAX_CLIENTS = 5;
const DWORD SOCKET_TIMEOUT_MS = 10000;

// Protocol Constants
enum JobType { TEXT_ANALYSIS = 1, PATTERN_MATCH = 2, IMAGE_PROCESS = 3 };

// Utility Functions
void setColor(int color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void printLog(const string& message, int color) {
    auto now = chrono::system_clock::now();
    auto now_c = chrono::system_clock::to_time_t(now);

    tm now_tm;
    localtime_s(&now_tm, &now_c);  // Safe replacement for localtime()

    ostringstream oss;
    oss << this_thread::get_id();
    string threadId = oss.str();

    setColor(color);
    cout << "[" << put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "] [THREAD " << threadId << "] [WORKER] " << message << endl;
    setColor(7); // White
}


// Network Functions
bool sendAll(SOCKET s, const char* data, int length) {
    int totalSent = 0;
    while (totalSent < length) {
        int sent = send(s, data + totalSent, length - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool receiveAll(SOCKET s, char* buffer, int length) {
    int totalReceived = 0;
    while (totalReceived < length) {
        int received = recv(s, buffer + totalReceived, length - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

// Request Processing
string processTextAnalysis(const string& text) {
    vector<string> positiveWords = { "good", "happy", "great" };
    vector<string> negativeWords = { "bad", "sad", "terrible" };

    int positive = 0, negative = 0;
    string lowerText = text;
    transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    for (const auto& word : positiveWords)
        if (lowerText.find(word) != string::npos) positive++;

    for (const auto& word : negativeWords)
        if (lowerText.find(word) != string::npos) negative++;

    if (positive > negative) return "Positive";
    if (negative > positive) return "Negative";
    return "Neutral";
}

string processPatternMatch(const string& text, const string& pattern) {
    size_t pos = text.find(pattern);
    if (pos != string::npos)
        return "Found at position: " + to_string(pos);
    return "Pattern not found";
}

void handleRequest(SOCKET clientSocket) {
    printLog("Starting request processing for client", 10); // Green

    char jobType;
    if (!receiveAll(clientSocket, &jobType, 1)) {
        printLog("Failed to receive job type from client", 4); // Red
        closesocket(clientSocket);
        return;
    }
    printLog("Received job type: " + to_string(static_cast<int>(jobType)), 10); // Green

    try {
        switch (static_cast<JobType>(jobType)) {
        case TEXT_ANALYSIS: {
            uint32_t textSize;
            receiveAll(clientSocket, reinterpret_cast<char*>(&textSize), sizeof(textSize));
            textSize = ntohl(textSize);
            printLog("Received text size: " + to_string(textSize), 10); // Green

            vector<char> textBuffer(textSize);
            receiveAll(clientSocket, textBuffer.data(), textSize);
            printLog("Received text for analysis", 10); // Green

            string result = processTextAnalysis(string(textBuffer.data(), textSize));
            printLog("Text analysis result: " + result, 10); // Green

            uint32_t resultSize = htonl(result.size());
            sendAll(clientSocket, reinterpret_cast<char*>(&resultSize), sizeof(resultSize));
            sendAll(clientSocket, result.c_str(), result.size());
            break;
        }

        case PATTERN_MATCH: {
            uint32_t patternSize, textSize;
            receiveAll(clientSocket, reinterpret_cast<char*>(&patternSize), sizeof(patternSize));
            patternSize = ntohl(patternSize);
            printLog("Received pattern size: " + to_string(patternSize), 10); // Green

            vector<char> patternBuffer(patternSize);
            receiveAll(clientSocket, patternBuffer.data(), patternSize);
            string pattern(patternBuffer.data(), patternSize);
            printLog("Received pattern: " + pattern, 10); // Green

            receiveAll(clientSocket, reinterpret_cast<char*>(&textSize), sizeof(textSize));
            textSize = ntohl(textSize);
            printLog("Received text size: " + to_string(textSize), 10); // Green

            vector<char> textBuffer(textSize);
            receiveAll(clientSocket, textBuffer.data(), textSize);
            printLog("Received text for pattern matching", 10); // Green

            string result = processPatternMatch(string(textBuffer.data(), textSize), pattern);
            printLog("Pattern match result: " + result, 10); // Green

            uint32_t resultSize = htonl(result.size());
            sendAll(clientSocket, reinterpret_cast<char*>(&resultSize), sizeof(resultSize));
            sendAll(clientSocket, result.c_str(), result.size());
            break;
        }

        default:
            throw runtime_error("Invalid job type");
        }
    }
    catch (const exception& e) {
        printLog("Processing error: " + string(e.what()), 4); // Red
    }

    printLog("Finished request processing for client", 10); // Green
    closesocket(clientSocket);
}

void startWorkerServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(WORKER_PORT);

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    listen(serverSocket, MAX_CLIENTS);

    printLog("Worker server started, listening on all interfaces on port " + to_string(WORKER_PORT), 10); // Green
    printLog("Maximum clients: " + to_string(MAX_CLIENTS), 10); // Green

    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET) {
            sockaddr_in clientAddr;
            int addrLen = sizeof(clientAddr);
            getpeername(clientSocket, (sockaddr*)&clientAddr, &addrLen);
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            int clientPort = ntohs(clientAddr.sin_port);
            printLog("Accepted connection from " + string(clientIP) + ":" + to_string(clientPort), 10); // Green
            thread(handleRequest, clientSocket).detach();
        }
    }
}

void registerWithManager() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    SOCKET managerSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in managerAddr{};
    managerAddr.sin_family = AF_INET;
    managerAddr.sin_port = htons(MANAGER_PORT);
    inet_pton(AF_INET, MANAGER_IP.c_str(), &managerAddr.sin_addr);

    printLog("Attempting to register with manager at " + MANAGER_IP + ":" + to_string(MANAGER_PORT), 10); // Green

    if (connect(managerSocket, (sockaddr*)&managerAddr, sizeof(managerAddr)) == 0) {
        uint32_t port = htonl(WORKER_PORT);
        if (send(managerSocket, reinterpret_cast<char*>(&port), sizeof(port), 0) == sizeof(port)) {
            printLog("Successfully registered with manager", 10); // Green
        }
        else {
            printLog("Failed to send port information to manager", 4); // Red
        }
    }
    else {
        printLog("Failed to connect to manager", 4); // Red
    }
    closesocket(managerSocket);
}

int main() {
    registerWithManager();
    startWorkerServer();
    WSACleanup();
    return 0;
}