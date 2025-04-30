#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")

// Console Color Constants
enum ConsoleColor {
    BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
    YELLOW = 6, WHITE = 7, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10
};

// Configuration
#define IMAGE_MANAGER_PORT 9092
#define BUFFER_SIZE 4096

// Utility Functions
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

// Network Functions
bool receiveAll(SOCKET s, char* buffer, int size) {
    int totalReceived = 0;
    while (totalReceived < size) {
        int received = recv(s, buffer + totalReceived, size - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
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

void handleClient(SOCKET clientSocket) {
    try {
        // Receive job type
        uint32_t jobTypeLen;
        if (!receiveAll(clientSocket, (char*)&jobTypeLen, sizeof(jobTypeLen))) {
            printError("Failed to receive job type length");
            return;
        }
        jobTypeLen = ntohl(jobTypeLen);

        std::vector<char> jobTypeBuffer(jobTypeLen);
        if (!receiveAll(clientSocket, jobTypeBuffer.data(), jobTypeLen)) {
            printError("Failed to receive job type");
            return;
        }
        std::string jobType(jobTypeBuffer.begin(), jobTypeBuffer.end());

        printHeader("Processing: " + jobType);

        // Receive image size
        uint32_t imageSize;
        if (!receiveAll(clientSocket, (char*)&imageSize, sizeof(imageSize))) {
            printError("Failed to receive image size");
            return;
        }
        imageSize = ntohl(imageSize);

        // Receive image data
        std::vector<uchar> buffer(imageSize);
        if (!receiveAll(clientSocket, (char*)buffer.data(), imageSize)) {
            printError("Failed to receive image data");
            return;
        }

        // Decode image
        cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
        if (image.empty()) {
            printError("Failed to decode image");
            return;
        }

        printSuccess("Received image: " + std::to_string(image.cols) + "x" +
            std::to_string(image.rows) + " pixels");

        cv::Mat processedImage;

        // Process based on job type
        if (jobType == "image_gray") {
            cv::cvtColor(image, processedImage, cv::COLOR_BGR2GRAY);
        }
        else if (jobType == "image_edge") {
            cv::Mat gray;
            cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
            cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);
            cv::Canny(gray, processedImage, 50, 150);
        }
        else if (jobType == "image_resize") {
            cv::resize(image, processedImage,
                cv::Size(image.cols / 2, image.rows / 2), // 50% resize
                0, 0, cv::INTER_LINEAR);
        }
        else if (jobType == "image_rotate") {
            cv::rotate(image, processedImage, cv::ROTATE_90_CLOCKWISE);
        }
        else if (jobType == "image_blur") {
            cv::GaussianBlur(image, processedImage, cv::Size(15, 15), 0);
        }
        else {
            printError("Unsupported job type: " + jobType);
            return;
        }

        // Encode processed image
        std::vector<uchar> outputBuffer;
        cv::imencode(".jpg", processedImage, outputBuffer);

        // Send processed image size
        uint32_t responseSize = htonl(outputBuffer.size());
        if (!sendAll(clientSocket, (char*)&responseSize, sizeof(responseSize))) {
            printError("Failed to send response size");
            return;
        }

        // Send processed image data
        if (!sendAll(clientSocket, (char*)outputBuffer.data(), outputBuffer.size())) {
            printError("Failed to send processed image");
            return;
        }

        printSuccess("Sent processed image (" + std::to_string(outputBuffer.size()) + " bytes)");
    }
    catch (...) {
        printError("Exception in image processing");
    }
}

int main() {
    WSADATA wsa;
    SOCKET serverSocket;
    sockaddr_in serverAddr{};

    setColor(CYAN);
    std::cout << "Initializing Image Processing Manager...\n";

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

    // Add socket reuse option
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printError("Setsockopt failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(IMAGE_MANAGER_PORT);

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

    printSuccess("Image Manager listening on port " + std::to_string(IMAGE_MANAGER_PORT));

    while (true) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET) {
            std::thread([clientSocket]() {
                handleClient(clientSocket);
                closesocket(clientSocket);
                }).detach();
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}