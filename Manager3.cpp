#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <cmath>
#include <sstream>
#include <climits>

#pragma comment(lib, "Ws2_32.lib")

enum ConsoleColor {
    BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
    YELLOW = 6, WHITE = 7, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10
};

#define ARRAY_MANAGER_PORT 9093
#define BUFFER_SIZE 4096

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

// Job Processing Functions
int medianOfMediansSelect(std::vector<int>& nums, int left, int right, int k);

int medianOfMedians(std::vector<int>& nums, int k) {
    return medianOfMediansSelect(nums, 0, nums.size() - 1, k);
}

int medianOfMediansSelect(std::vector<int>& nums, int left, int right, int k) {
    if (left == right) return nums[left];

    int pivotIndex = left + (right - left) / 2;
    pivotIndex = (pivotIndex + right) / 2; // Simplification for example

    std::swap(nums[pivotIndex], nums[right]);
    int pivot = nums[right];

    int i = left;
    for (int j = left; j < right; j++) {
        if (nums[j] <= pivot) {
            std::swap(nums[i], nums[j]);
            i++;
        }
    }
    std::swap(nums[i], nums[right]);

    if (k == i)
        return nums[i];
    else if (k < i)
        return medianOfMediansSelect(nums, left, i - 1, k);
    else
        return medianOfMediansSelect(nums, i + 1, right, k);
}

double computeMedianWithMedianOfMedians(std::vector<int> array) {
    if (array.empty()) return 0.0;
    size_t n = array.size();
    if (n % 2 == 1) {
        return medianOfMedians(array, n / 2);
    }
    else {
        int m1 = medianOfMedians(array, n / 2 - 1);
        int m2 = medianOfMedians(array, n / 2);
        return (m1 + m2) / 2.0;
    }
}

int computeGCD(int a, int b) {
    while (b != 0) {
        int temp = b;
        b = a % b;
        a = temp;
    }
    return a;
}

int computeArrayGCD(const std::vector<int>& array) {
    if (array.empty()) return 0;
    int result = array[0];
    for (size_t i = 1; i < array.size(); ++i) {
        result = computeGCD(result, array[i]);
        if (result == 1) break;
    }
    return result;
}

int computeKadaneMaxSum(const std::vector<int>& array) {
    if (array.empty()) return 0;
    int maxCurrent = array[0];
    int maxGlobal = array[0];
    for (size_t i = 1; i < array.size(); ++i) {
        maxCurrent = max(array[i], maxCurrent + array[i]);
        maxGlobal = max(maxGlobal, maxCurrent);
    }
    return maxGlobal;
}

void rotateArray(std::vector<int>& array, int k) {
    if (array.empty()) return;
    k %= array.size();
    if (k < 0) k += array.size();
    std::reverse(array.begin(), array.end());
    std::reverse(array.begin(), array.begin() + k);
    std::reverse(array.begin() + k, array.end());
}

bool sendAll(SOCKET s, const char* buffer, int size) {
    int totalSent = 0;
    while (totalSent < size) {
        int sent = send(s, buffer + totalSent, size - totalSent, 0);
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
        printHeader("New Client Request");

        // Receive job type length
        uint32_t jobTypeLength;
        if (!receiveAll(clientSocket, (char*)&jobTypeLength, sizeof(jobTypeLength))) {
            printError("Failed to receive job type length");
            return;
        }
        jobTypeLength = ntohl(jobTypeLength);

        // Receive job type
        std::vector<char> jobTypeBuffer(jobTypeLength);
        if (!receiveAll(clientSocket, jobTypeBuffer.data(), jobTypeLength)) {
            printError("Failed to receive job type");
            return;
        }
        std::string jobType(jobTypeBuffer.begin(), jobTypeBuffer.end());

        // Receive array size
        uint32_t arraySize;
        if (!receiveAll(clientSocket, (char*)&arraySize, sizeof(arraySize))) {
            printError("Failed to receive array size");
            return;
        }
        arraySize = ntohl(arraySize);

        // Receive array elements
        std::vector<int> array;
        for (uint32_t i = 0; i < arraySize; ++i) {
            uint32_t num;
            if (!receiveAll(clientSocket, (char*)&num, sizeof(num))) {
                printError("Failed to receive array element");
                return;
            }
            array.push_back(ntohl(num));
        }

        // Log received job
        std::stringstream ss;
        ss << "Processing job: " << jobType << " with array size " << array.size();
        printSuccess(ss.str());

        std::string result;

        if (jobType == "array_quick_sort") {
            std::vector<int> sortedArray = array;
            std::sort(sortedArray.begin(), sortedArray.end());
            std::stringstream ss;
            for (size_t i = 0; i < sortedArray.size(); ++i) {
                if (i > 0) ss << ",";
                ss << sortedArray[i];
            }
            result = ss.str();
        }
        else if (jobType == "array_median_median") {
            if (array.empty()) {
                result = "Error: Empty array";
            }
            else {
                double median = computeMedianWithMedianOfMedians(array);
                result = std::to_string(median);
            }
        }
        else if (jobType == "array_gcd") {
            if (array.empty()) {
                result = "Error: Empty array";
            }
            else {
                int gcd = computeArrayGCD(array);
                result = std::to_string(gcd);
            }
        }
        else if (jobType == "array_kadane") {
            if (array.empty()) {
                result = "Error: Empty array";
            }
            else {
                int maxSum = computeKadaneMaxSum(array);
                result = std::to_string(maxSum);
            }
        }
        else if (jobType == "array_rotate") {
            if (array.size() < 1) {
                result = "Error: Rotation count missing";
            }
            else {
                int k = array[0];
                std::vector<int> data(array.begin() + 1, array.end());
                if (data.empty()) {
                    result = "Error: No elements to rotate";
                }
                else {
                    rotateArray(data, k);
                    std::stringstream ss;
                    for (size_t i = 0; i < data.size(); ++i) {
                        if (i > 0) ss << ",";
                        ss << data[i];
                    }
                    result = ss.str();
                }
            }
        }
        else {
            result = "Error: Unknown job type";
        }

        // Send result
        uint32_t resultSize = htonl(result.size());
        if (!sendAll(clientSocket, (char*)&resultSize, sizeof(resultSize)) ||
            !sendAll(clientSocket, result.c_str(), result.size())) {
            printError("Failed to send result");
        }
        else {
            printSuccess("Result sent: " + result);
        }
    }
    catch (...) {
        printError("Exception in client handling");
    }
}

int main() {
    WSADATA wsa;
    SOCKET serverSocket;
    sockaddr_in serverAddr{};

    setColor(CYAN);
    std::cout << "Initializing Array Manager...\n";

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

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printError("Setsockopt failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(ARRAY_MANAGER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printError("Bind failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    listen(serverSocket, 5);
    printSuccess("Manager listening on port " + std::to_string(ARRAY_MANAGER_PORT));

    while (true) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET) {
            handleClient(clientSocket);
            closesocket(clientSocket);
        }
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}