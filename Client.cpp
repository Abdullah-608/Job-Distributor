#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <opencv2/opencv.hpp>
#include <vector>
#include <cstring>
#include <conio.h>
#include <windows.h>
#include <sstream>
#include <thread>
#include <atomic>
#include <random>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
using namespace cv;

// Console color constants
enum ConsoleColor {
    BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5, YELLOW = 6,
    WHITE = 7, GRAY = 8, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10
};

// ================== UTILITY FUNCTIONS ==================
void setColor(ConsoleColor color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void printHeader(const string& text) {
    setColor(BRIGHT_BLUE);
    cout << "\n=== " << text << " ===\n";
    setColor(WHITE);
}

void printSuccess(const string& message) {
    setColor(BRIGHT_GREEN);
    cout << "[SUCCESS] " << message << endl;
    setColor(WHITE);
}

void printError(const string& message) {
    setColor(RED);
    cerr << "[ERROR] " << message << endl;
    setColor(WHITE);
}

void printInfo(const string& message) {
    setColor(CYAN);
    cout << "[INFO] " << message << endl;
    setColor(WHITE);
}

// ================== NETWORK FUNCTIONS ==================
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

atomic<int> activeThreads(0);

bool sendAll(SOCKET s, const char* data, int length) {
    int totalSent = 0;
    while (totalSent < length) {
        int sent = send(s, data + totalSent, length - totalSent, 0);
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

bool sendJobType(SOCKET s, const string& jobType) {
    uint32_t typeLen = htonl(jobType.size());
    return sendAll(s, (char*)&typeLen, sizeof(typeLen)) &&
        sendAll(s, jobType.c_str(), jobType.size());
}

SOCKET createAndConnectSocket() {
    WSADATA wsa;
    SOCKET s;
    sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printError("WSAStartup failed!");
        return INVALID_SOCKET;
    }

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printError("Socket creation failed!");
        WSACleanup();
        return INVALID_SOCKET;
    }

    server.sin_family = AF_INET;
    inet_pton(AF_INET, SERVER_IP, &server.sin_addr);
    server.sin_port = htons(SERVER_PORT);

    if (connect(s, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        printError("Connection failed!");
        closesocket(s);
        WSACleanup();
        return INVALID_SOCKET;
    }

    return s;
}

// ================== JOB HANDLING FUNCTIONS ==================
void sendTextJob(SOCKET s, const string& jobType, const string& text, int clientId = 0) {
    string prefix = clientId > 0 ? "[Client " + to_string(clientId) + "] " : "";

    // Prepare buffer from text
    vector<char> buffer(text.begin(), text.end());
    uint32_t dataSize = htonl(buffer.size());

    // Send job type, size, and text data
    if (!sendJobType(s, jobType) ||
        !sendAll(s, (char*)&dataSize, sizeof(uint32_t)) ||
        !sendAll(s, buffer.data(), buffer.size())) {
        printError(prefix + "Text send failed");
        return;
    }

    // Receive response size
    uint32_t responseSize;
    if (!receiveAll(s, (char*)&responseSize, sizeof(responseSize))) {
        printError(prefix + "Response size error");
        return;
    }
    responseSize = ntohl(responseSize);

    // Receive response data
    vector<char> responseBuffer(responseSize);
    if (!receiveAll(s, responseBuffer.data(), responseSize)) {
        printError(prefix + "Response data error");
        return;
    }

    string result(responseBuffer.begin(), responseBuffer.end());
    clientId == 0 ? printSuccess("Result: " + result)
        : printInfo(prefix + "Received: " + result);
}


void sendImageJob(SOCKET s, const string& jobType, const string& path, int clientId = 0) {
    string prefix = clientId > 0 ? "[Client " + to_string(clientId) + "] " : "";
    Mat img = imread(path, IMREAD_COLOR);

    if (img.empty()) {
        printError(prefix + "Failed to load: " + path);
        return;
    }

    vector<uchar> buffer;
    imencode(".jpg", img, buffer);

    uint32_t dataSize = htonl(buffer.size());

    if (!sendJobType(s, jobType) ||
        !sendAll(s, (char*)&dataSize, sizeof(uint32_t)) ||
        !sendAll(s, (char*)buffer.data(), buffer.size())) {
        printError(prefix + "Image send failed");
        return;
    }


    uint32_t responseSize;
    if (!receiveAll(s, (char*)&responseSize, sizeof(responseSize))) {
        printError(prefix + "Response size error");
        return;
    }
    responseSize = ntohl(responseSize);

    vector<uchar> resultImg(responseSize);
    if (!receiveAll(s, (char*)resultImg.data(), responseSize)) {
        printError(prefix + "Image receive failed");
        return;
    }

    string outPath = "result_" + jobType + "_" + to_string(clientId) + ".jpg";
    imwrite(outPath, imdecode(resultImg, IMREAD_COLOR));
    clientId == 0 ? printSuccess("Saved: " + outPath)
        : printInfo(prefix + "Saved: " + outPath);
}

void sendArrayJob(SOCKET s, const string& jobType, const vector<int>& arr, int clientId = 0) {
    string prefix = clientId > 0 ? "[Client " + to_string(clientId) + "] " : "";

    // Convert integers to network byte order
    vector<uint32_t> netArr;
    for (int num : arr)
        netArr.push_back(htonl(static_cast<uint32_t>(num)));

    // Compute byte size of the array
    uint32_t dataSize = htonl(netArr.size() * sizeof(uint32_t));

    // Send job type, data size, and the array
    if (!sendJobType(s, jobType) ||
        !sendAll(s, (char*)&dataSize, sizeof(uint32_t)) ||
        !sendAll(s, (char*)netArr.data(), netArr.size() * sizeof(uint32_t))) {
        printError(prefix + "Array send failed");
        return;
    }

    // Receive response size
    uint32_t responseSize;
    if (!receiveAll(s, (char*)&responseSize, sizeof(responseSize))) {
        printError(prefix + "Response size error");
        return;
    }
    responseSize = ntohl(responseSize);

    if (jobType.find("array_") != string::npos) {
        // Receive array result
        vector<uint32_t> netResult(responseSize / sizeof(uint32_t));
        if (!receiveAll(s, (char*)netResult.data(), responseSize)) {
            printError(prefix + "Array receive failed");
            return;
        }

        // Convert result back to host byte order
        vector<int> result;
        for (auto n : netResult)
            result.push_back(ntohl(n));

        if (clientId == 0) {
            cout << "Result: [";
            for (size_t i = 0; i < result.size(); ++i) {
                cout << result[i] << (i < result.size() - 1 ? ", " : "");
            }
            cout << "]\n";
            printSuccess("Array operation completed");
        }
        else {
            printInfo(prefix + "Received array");
        }
    }
    else {
        // Receive single value result
        uint32_t result;
        if (!receiveAll(s, (char*)&result, sizeof(result))) {
            printError(prefix + "Value receive failed");
            return;
        }
        result = ntohl(result);
        clientId == 0 ? printSuccess("Result: " + to_string(result))
            : printInfo(prefix + "Result: " + to_string(result));
    }
}


// ================== CUSTOM JOB INTERFACES ==================
void customTextProcessing() {
    SOCKET s = createAndConnectSocket();
    if (s == INVALID_SOCKET) return;

    printHeader("TEXT PROCESSING");
    cout << "1. Sentiment Analysis\n"
        << "2. Language Detection\n"
        << "3. Spam Detection\n"
        << "4. Text Summary\n"
        << "5. Entity Recognition\n"
        << "Select job (1-5): ";

    int choice;
    cin >> choice; cin.ignore();

    string types[] = { "sentiment", "language", "spam", "summary", "entities" };
    if (choice < 1 || choice > 5) {
        printError("Invalid choice!");
        closesocket(s);
        return;
    }

    printHeader("ENTER TEXT");
    string text;
    cout << "Input text:\n";
    getline(cin, text);

    sendTextJob(s, "text_" + types[choice - 1], text);
    closesocket(s);
}

void customImageProcessing() {
    SOCKET s = createAndConnectSocket();
    if (s == INVALID_SOCKET) return;

    printHeader("IMAGE PROCESSING");
    cout << "1. Grayscale\n"
        << "2. Edge Detection\n"
        << "3. Resize\n"
        << "4. Rotate\n"
        << "5. Blur\n"
        << "Select job (1-5): ";

    int choice;
    cin >> choice; cin.ignore();

    string types[] = { "gray", "edge", "resize", "rotate", "blur" };
    if (choice < 1 || choice > 5) {
        printError("Invalid choice!");
        closesocket(s);
        return;
    }

    printHeader("IMAGE INPUT");
    string path;
    cout << "Image path (Enter for default): ";
    getline(cin, path);
    if (path.empty()) path = "test.jpg";

    sendImageJob(s, "image_" + types[choice - 1], path);
    closesocket(s);
}
void customArrayProcessing() {
    SOCKET s = createAndConnectSocket();
    if (s == INVALID_SOCKET) return;

    printHeader("ARRAY PROCESSING");
    cout << "1. Quick Sort\n"
        << "2. Median (Median of Medians)\n"
        << "3. GCD (Euclidean Algorithm)\n"
        << "4. Max Subarray Sum (Kadane's Algorithm)\n"
        << "5. Rotate Array\n"
        << "Select job (1-5): ";

    int choice;
    cin >> choice;

    string types[] = { "quick_sort", "median_median", "gcd", "kadane", "rotate" };
    if (choice < 1 || choice > 5) {
        printError("Invalid choice!");
        closesocket(s);
        return;
    }

    printHeader("ARRAY INPUT");
    int size;
    cout << "Array size: ";
    cin >> size;

    vector<int> arr;
    cout << "Enter " << size << " numbers:\n";
    for (int i = 0; i < size; ++i) {
        int num;
        cin >> num;
        arr.push_back(num);
    }

    sendArrayJob(s, "array_" + types[choice - 1], arr);
    closesocket(s);
}


// ================== DEFAULT REQUESTS ==================
vector<string> generateTexts() {
    return {
     "I love programming!",
     "This is spam content",
     "Bonjour le monde",
     "A long detailed text about technology and its impact on society.",
     "John Doe visited Paris",
     "Get rich quick by clicking this link!",
     "Hola amigo, cómo estás?",
     "Subscribe to our channel for daily updates!",
     "Alice went to Wonderland",
     "Ceci n'est pas une pipe",
     "Win a free iPhone now!",
     "Machine learning is fascinating.",
     "This message contains malware.",
     "Hello World!",
     "Je suis étudiant en informatique.",
     "Breaking news: Market crashes!",
     "Python is a powerful language.",
     "Limited time offer just for you!",
     "Guten Morgen, wie geht’s dir?",
     "Contact us at support@example.com",
     "Data science is the future.",
     "Earn money from home easily!",
     "Rome wasn’t built in a day.",
     "The Eiffel Tower is in Paris.",
     "Dangerous content detected.",
     "JavaScript powers the web.",
     "This is not a drill!",
     "Visit our website now!",
     "Artificial Intelligence will change the world.",
     "Click here for instant access!",
     "こんにちは、元気ですか？",
     "The Great Wall of China is massive.",
     "Spam messages are annoying.",
     "Learn C++ in 30 days!",
     "Donald Trump visited Tokyo.",
     "Free coupons inside!",
     "React is a JavaScript library.",
     "You’ve won a lottery ticket!",
     "La vita è bella.",
     "Contact John at john.doe@example.com",
     "Fast weight loss secrets revealed!",
     "Quantum computing is amazing.",
     "This is a phishing attempt.",
     "Mount Everest is the tallest mountain.",
     "Hackers target bank accounts.",
     "Good morning, have a nice day!",
     "Zeus was the king of the gods.",
     "Download our app for free!",
     "Life is like a box of chocolates.",
     "Congratulations! You've been selected!"
    };

}

vector<vector<int>> generateArrays() {
    return {
        {5,2,9}, {1,2,3}, {10,20,30}, {4,5,6}, {7,8,9}
    };
}

vector<string> generateImagePaths() {
    return { "test.jpg", "test.jpg", "test.jpg", "test.jpg", "test.jpg","test.jpg", "test.jpg", "test.jpg", "test.jpg", "test.jpg" };
}

void simulateClient(int id, const string& type, const string& data) {
    activeThreads++;
    SOCKET s = createAndConnectSocket();
    if (s == INVALID_SOCKET) {
        activeThreads--;
        return;
    }

    try {
        if (type == "text") {
            // Possible text-related jobs
            vector<string> textJobs = {
                "text_sentiment", "text_language", "text_spam", "text_summary", "text_entities"
            };
            string randomJob = textJobs[rand() % textJobs.size()];
            sendTextJob(s, randomJob, data, id);
        }
        else if (type == "image") {
            // Possible image-related jobs
            vector<string> imageJobs = {
               "image_gray", "image_edge", "image_resize", "image_rotate", "image_blur"
            };
            string randomJob = imageJobs[rand() % imageJobs.size()];
            sendImageJob(s, randomJob, data, id);
        }
        else if (type == "array") {
            vector<int> arr;
            stringstream ss(data);
            string num;
            while (getline(ss, num, ','))
                arr.push_back(stoi(num));
            sendArrayJob(s, "array_sort", arr, id);
        }
    }
    catch (...) {
        printError("Client " + to_string(id) + " failed!");
    }

    closesocket(s);
    activeThreads--;
}

void sendDefaultRequests() {
    vector<thread> threads;
    int clientId = 1;

    for (const auto& text : generateTexts()) {
        threads.emplace_back(simulateClient, clientId++, "text", text);
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    for (const auto& img : generateImagePaths()) {
        threads.emplace_back(simulateClient, clientId++, "image", img);
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    /*
    for (const auto& arr : generateArrays()) {
        stringstream ss;
        for (size_t i = 0; i < arr.size(); ++i) {
            ss << arr[i] << (i < arr.size() - 1 ? "," : "");
        }
        threads.emplace_back(simulateClient, clientId++, "array", ss.str());
        this_thread::sleep_for(chrono::milliseconds(50));
    }
    */

    for (auto& t : threads) t.join();
    printSuccess("Default requests completed");
}

// ================== MAIN FUNCTION ==================
int main() {
    while (true) {
        system("cls");
        printHeader("DISTRIBUTED PROCESSING CLIENT");
        cout << "1. Custom Text Processing\n"
            << "2. Custom Image Processing\n"
            << "3. Custom Array Processing\n"
            << "4. Send Default Requests\n"
            << "5. Exit\n"
            << "Choice: ";

        int choice;
        cin >> choice;

        switch (choice) {
        case 1: {
            // Collect input in main thread
            printHeader("TEXT PROCESSING");
            cout << "1. Sentiment Analysis\n"
                << "2. Language Detection\n"
                << "3. Spam Detection\n"
                << "4. Text Summary\n"
                << "5. Entity Recognition\n"
                << "Select job (1-5): ";

            int jobChoice;
            cin >> jobChoice; cin.ignore();
            string types[] = { "sentiment", "language", "spam", "summary", "entities" };

            if (jobChoice < 1 || jobChoice > 5) {
                printError("Invalid choice!");
                break;
            }

            printHeader("ENTER TEXT");
            string text;
            cout << "Input text:\n";
            getline(cin, text);

            // Launch job in thread
            thread([jobChoice, text, types]() {
                activeThreads++;
                SOCKET s = createAndConnectSocket();
                if (s != INVALID_SOCKET) {
                    sendTextJob(s, "text_" + types[jobChoice - 1], text);
                    closesocket(s);
                }
                activeThreads--;
                }).detach();
                break;
        }

        case 2: {
            printHeader("IMAGE PROCESSING");
            cout << "1. Grayscale\n"
                << "2. Edge Detection\n"
                << "3. Resize\n"
                << "4. Rotate\n"
                << "5. Blur\n"
                << "Select job (1-5): ";

            int jobChoice;
            cin >> jobChoice; cin.ignore();
            string types[] = { "gray", "edge", "resize", "rotate", "blur" };

            if (jobChoice < 1 || jobChoice > 5) {
                printError("Invalid choice!");
                break;
            }

            printHeader("IMAGE INPUT");
            string path;
            cout << "Image path (Enter for default): ";
            getline(cin, path);
            if (path.empty()) path = "test.jpg";

            thread([jobChoice, path, types]() {
                activeThreads++;
                SOCKET s = createAndConnectSocket();
                if (s != INVALID_SOCKET) {
                    sendImageJob(s, "image_" + types[jobChoice - 1], path);
                    closesocket(s);
                }
                activeThreads--;
                }).detach();
                break;
        }

        case 3: {
            printHeader("ARRAY PROCESSING");
            cout << "1. Quick Sort\n"
                << "2. Median (Median of Medians)\n"
                << "3. GCD (Euclidean Algorithm)\n"
                << "4. Max Subarray Sum (Kadane's Algorithm)\n"
                << "5. Rotate Array\n"
                << "Select job (1-5): ";

            int jobChoice;
            cin >> jobChoice;
            string types[] = { "quick_sort", "median_median", "gcd", "kadane", "rotate" };

            if (jobChoice < 1 || jobChoice > 5) {
                printError("Invalid choice!");
                break;
            }

            printHeader("ARRAY INPUT");
            int size;
            cout << "Array size: ";
            cin >> size;

            vector<int> arr;
            cout << "Enter " << size << " numbers:\n";
            for (int i = 0; i < size; ++i) {
                int num;
                cin >> num;
                arr.push_back(num);
            }

            thread([jobChoice, arr, types]() {
                activeThreads++;
                SOCKET s = createAndConnectSocket();
                if (s != INVALID_SOCKET) {
                    sendArrayJob(s, "array_" + types[jobChoice - 1], arr);
                    closesocket(s);
                }
                activeThreads--;
                }).detach();
                break;
        }

        case 4:
            thread(sendDefaultRequests).detach();
            break;

        case 5:
            while (activeThreads > 0) this_thread::sleep_for(1s);
            return 0;

        default:
            printError("Invalid choice!");
        }

        cout << "\nPress any key...";
        _getch();
    }
}
