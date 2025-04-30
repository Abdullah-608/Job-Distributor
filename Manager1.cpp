#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <thread>
#include <sstream>
#include <map>
#include <cctype>
#include <locale>

#pragma comment(lib, "Ws2_32.lib")

// Console Color Constants
enum ConsoleColor {
    BLUE = 1, GREEN = 2, CYAN = 3, RED = 4, MAGENTA = 5,
    YELLOW = 6, WHITE = 7, BRIGHT_BLUE = 9, BRIGHT_GREEN = 10
};

// Configuration
#define TEXT_MANAGER_PORT 9091
#define MAX_TEXT_SIZE 65535  // 64KB

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

void printInfo(const std::string& message) {
    setColor(CYAN);
    std::cout << "[INFO] " << message << std::endl;
    setColor(WHITE);
}

// Network Functions
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

// Text Processing Functions
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <sstream>

std::string analyzeSentiment(const std::string& text) {
    // Using unordered_map for faster lookups
    static const std::unordered_map<std::string, int> positiveWords = {
        {"good", 1}, {"great", 2}, {"excellent", 3}, {"wonderful", 2},
        {"happy", 2}, {"love", 3}, {"awesome", 2}, {"fantastic", 3},
        {"amazing", 2}, {"perfect", 3}, {"joy", 2}, {"delight", 2},
        {"pleasure", 1}, {"satisfied", 1}, {"brilliant", 2}, {"superb", 2}
    };

    static const std::unordered_map<std::string, int> negativeWords = {
        {"bad", 1}, {"terrible", 3}, {"awful", 2}, {"hate", 3},
        {"worst", 3}, {"angry", 2}, {"sad", 2}, {"disappointing", 2},
        {"horrible", 3}, {"painful", 2}, {"upset", 1}, {"frustrated", 2},
        {"annoying", 1}, {"disgusting", 2}, {"miserable", 2}, {"depressed", 2}
    };

    // More granular sentiment thresholds
    constexpr int VERY_POSITIVE_THRESHOLD = 8;
    constexpr int POSITIVE_THRESHOLD = 3;
    constexpr int VERY_NEGATIVE_THRESHOLD = -8;
    constexpr int NEGATIVE_THRESHOLD = -3;

    int sentimentScore = 0;
    size_t wordCount = 0;

    // Tokenize the text into words for more accurate matching
    std::istringstream iss(text);
    std::string word;

    while (iss >> word) {
        // Remove punctuation and convert to lowercase
        word.erase(std::remove_if(word.begin(), word.end(),
            [](char c) { return std::ispunct(c); }), word.end());
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);

        if (word.empty()) continue;
        wordCount++;

        // Check positive words
        auto posIt = positiveWords.find(word);
        if (posIt != positiveWords.end()) {
            sentimentScore += posIt->second;
        }

        // Check negative words
        auto negIt = negativeWords.find(word);
        if (negIt != negativeWords.end()) {
            sentimentScore -= negIt->second;
        }
    }

    // Normalize score by word count if there are words
    if (wordCount > 0) {
        double normalizedScore = static_cast<double>(sentimentScore) / wordCount;

        if (normalizedScore > 0.5) return "Very Positive";
        if (normalizedScore > 0.2) return "Positive";
        if (normalizedScore < -0.5) return "Very Negative";
        if (normalizedScore < -0.2) return "Negative";
    }

    return "Neutral";
}

std::string detectLanguage(const std::string& text) {
    // Language detection using common words
    std::map<std::string, std::vector<std::string>> langKeywords = {
        {"English", {" the ", " and ", " to ", " of ", " a ", " in ", " is ", " that "}},
        {"Spanish", {" el ", " y ", " de ", " que ", " en ", " la ", " los ", " las "}},
        {"French", {" le ", " et ", " de ", " les ", " des ", " en ", " un ", " une "}},
        {"German", {" der ", " und ", " die ", " in ", " den ", " das ", " ein ", " eine "}},
        {"Italian", {" il ", " e ", " di ", " che ", " in ", " la ", " un ", " una "}}
    };

    std::string lowerText = " " + text + " ";
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    std::map<std::string, int> scores;
    // C++11 compatible version
    for (const auto& langEntry : langKeywords) {
        const std::string& lang = langEntry.first;
        const std::vector<std::string>& words = langEntry.second;
        for (const auto& word : words) {
            size_t pos = 0;
            while ((pos = lowerText.find(word, pos)) != std::string::npos) {
                scores[lang]++;
                pos += word.length();
                if (pos >= lowerText.length()) break;
            }
        }
    }

    if (scores.empty()) return "Unknown";

    auto maxLang = std::max_element(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    return maxLang->first + " (confidence: " + std::to_string(maxLang->second) + ")";
}

std::string checkSpam(const std::string& text) {
    const std::vector<std::string> spamIndicators = {
        "free", "win", "prize", "urgent", "cash",
        "offer", "limited", "click", "congratulations",
        "guaranteed", "risk-free", "winner", "selected",
        "discount", "deal", "promo", "bonus"
    };

    const std::vector<std::string> spamPatterns = {
        "only $", "!!!", "call now", "click here",
        "dear friend", "credit card", "password", "account"
    };

    std::string lowerText = text;
    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

    int spamScore = 0;
    for (const auto& word : spamIndicators) {
        if (lowerText.find(word) != std::string::npos) {
            spamScore += 2;
        }
    }

    for (const auto& pattern : spamPatterns) {
        if (lowerText.find(pattern) != std::string::npos) {
            spamScore += 3;
        }
    }

    // Check for excessive punctuation
    int exclamationCount = std::count(text.begin(), text.end(), '!');
    spamScore += exclamationCount > 3 ? exclamationCount : 0;

    if (spamScore > 10) return "Definitely SPAM (score: " + std::to_string(spamScore) + ")";
    if (spamScore > 5) return "Likely SPAM (score: " + std::to_string(spamScore) + ")";
    return "Not spam (score: " + std::to_string(spamScore) + ")";
}

std::string generateSummary(const std::string& text) {
    // Simple extractive summarization (first 3 sentences + keywords)
    std::istringstream iss(text);
    std::string sentence;
    std::vector<std::string> sentences;

    while (std::getline(iss, sentence, '.')) {
        if (!sentence.empty()) {
            // Trim whitespace
            sentence.erase(sentence.begin(), std::find_if(sentence.begin(), sentence.end(), [](int ch) { return !std::isspace(ch); }));
            sentence.erase(std::find_if(sentence.rbegin(), sentence.rend(), [](int ch) { return !std::isspace(ch); }).base(), sentence.end());

            if (!sentence.empty()) {
                sentences.push_back(sentence.substr(0, 150) + ".");
            }
        }
    }

    if (sentences.empty()) return "No summary generated";

    // Extract important words
    std::map<std::string, int> wordCounts;
    std::istringstream wordStream(text);
    std::string word;
    while (wordStream >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());
        if (word.length() > 3) {  // Ignore short words
            std::transform(word.begin(), word.end(), word.begin(), ::tolower);
            wordCounts[word]++;
        }
    }

    // Get top 3 keywords
    std::vector<std::pair<std::string, int>> sortedWords(wordCounts.begin(), wordCounts.end());
    std::sort(sortedWords.begin(), sortedWords.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    std::string keywords;
    for (size_t i = 0; i < std::min<size_t>(3, sortedWords.size()); ++i) {
        keywords += sortedWords[i].first + ", ";
    }

    size_t summaryLength = std::min<size_t>(3, sentences.size());
    std::string summary;
    for (size_t i = 0; i < summaryLength; ++i) {
        summary += sentences[i] + " ";
    }

    return summary + "\nKeywords: " + keywords.substr(0, keywords.length() - 2);
}

std::string recognizeEntities(const std::string& text) {
    // Improved entity recognition (proper nouns and known entities)
    std::istringstream iss(text);
    std::string word;
    std::vector<std::string> people;
    std::vector<std::string> places;
    std::vector<std::string> organizations;
    std::vector<std::string> dates;
    std::vector<std::string> other;

    // Common entity lists (would be better with a proper NLP library)
    const std::vector<std::string> commonPlaces = {
        "New York", "London", "Paris", "Tokyo", "Berlin",
        "Washington", "Beijing", "Moscow", "Rome", "Madrid"
    };

    const std::vector<std::string> commonOrgs = {
        "Google", "Microsoft", "Apple", "Amazon", "Facebook",
        "IBM", "Tesla", "NASA", "UN", "WHO"
    };

    bool prevWordCapitalized = false;
    std::string entityBuffer;

    auto processBuffer = [&]() {
        if (!entityBuffer.empty()) {
            std::string entity = entityBuffer;
            // Check against known entities
            if (std::find(commonPlaces.begin(), commonPlaces.end(), entity) != commonPlaces.end()) {
                places.push_back(entity);
            }
            else if (std::find(commonOrgs.begin(), commonOrgs.end(), entity) != commonOrgs.end()) {
                organizations.push_back(entity);
            }
            else if (entity.find("Inc.") != std::string::npos ||
                entity.find("Corp") != std::string::npos) {
                organizations.push_back(entity);
            }
            else {
                people.push_back(entity);
            }
            entityBuffer.clear();
        }
        };

    while (iss >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());

        if (!word.empty() && isupper(word[0])) {
            if (prevWordCapitalized) {
                entityBuffer += " " + word;
            }
            else {
                processBuffer();
                entityBuffer = word;
            }
            prevWordCapitalized = true;
        }
        else {
            processBuffer();
            prevWordCapitalized = false;
        }
    }
    processBuffer();

    // Format results
    std::ostringstream oss;
    if (!people.empty()) {
        oss << "People: ";
        for (const auto& p : people) oss << p << ", ";
        oss << "\n";
    }
    if (!places.empty()) {
        oss << "Places: ";
        for (const auto& p : places) oss << p << ", ";
        oss << "\n";
    }
    if (!organizations.empty()) {
        oss << "Organizations: ";
        for (const auto& o : organizations) oss << o << ", ";
        oss << "\n";
    }

    std::string result = oss.str();
    return result.empty() ? "No entities found" : result.substr(0, result.length() - 2);
}

void handleClient(SOCKET clientSocket) {
    try {
        // Set timeout (5 seconds)
        DWORD timeout = 5000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

        // Get client info for logging
        sockaddr_in clientInfo;
        int addrSize = sizeof(clientInfo);
        getpeername(clientSocket, (sockaddr*)&clientInfo, &addrSize);
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, INET_ADDRSTRLEN);

        // Receive job type length
        uint32_t jobTypeLen;
        if (!receiveAll(clientSocket, (char*)&jobTypeLen, sizeof(jobTypeLen))) {
            printError("[" + std::string(clientIP) + "] Job type length receive failed");
            closesocket(clientSocket);
            return;
        }
        jobTypeLen = ntohl(jobTypeLen);

        // Validate job type length
        if (jobTypeLen > 100 || jobTypeLen == 0) {
            printError("[" + std::string(clientIP) + "] Invalid job type length: " + std::to_string(jobTypeLen));
            closesocket(clientSocket);
            return;
        }

        // Receive job type
        std::vector<char> jobTypeBuffer(jobTypeLen);
        if (!receiveAll(clientSocket, jobTypeBuffer.data(), jobTypeLen)) {
            printError("[" + std::string(clientIP) + "] Job type receive failed");
            closesocket(clientSocket);
            return;
        }
        std::string jobType(jobTypeBuffer.begin(), jobTypeBuffer.end());

        // Validate job type
        const std::vector<std::string> validJobTypes = {
            "text_sentiment", "text_language", "text_spam",
            "text_summary", "text_entities"
        };

        if (std::find(validJobTypes.begin(), validJobTypes.end(), jobType) == validJobTypes.end()) {
            printError("[" + std::string(clientIP) + "] Invalid job type: " + jobType);
            closesocket(clientSocket);
            return;
        }

        // Receive data size
        uint32_t dataSize;
        if (!receiveAll(clientSocket, (char*)&dataSize, sizeof(dataSize))) {
            printError("[" + std::string(clientIP) + "] Data size receive failed");
            closesocket(clientSocket);
            return;
        }
        dataSize = ntohl(dataSize);

        // Validate data size
        if (dataSize > MAX_TEXT_SIZE || dataSize == 0) {
            printError("[" + std::string(clientIP) + "] Invalid data size: " + std::to_string(dataSize));
            closesocket(clientSocket);
            return;
        }

        // Receive text data
        std::vector<char> buffer(dataSize);
        if (!receiveAll(clientSocket, buffer.data(), dataSize)) {
            printError("[" + std::string(clientIP) + "] Text data receive failed");
            closesocket(clientSocket);
            return;
        }
        std::string text(buffer.begin(), buffer.end());

        printInfo("[" + std::string(clientIP) + "] Processing " + jobType + " (" +
            std::to_string(dataSize) + " bytes)");

        // Process request
        std::string result;
        if (jobType == "text_sentiment") {
            result = analyzeSentiment(text);
        }
        else if (jobType == "text_language") {
            result = detectLanguage(text);
        }
        else if (jobType == "text_spam") {
            result = checkSpam(text);
        }
        else if (jobType == "text_summary") {
            result = generateSummary(text);
        }
        else if (jobType == "text_entities") {
            result = recognizeEntities(text);
        }

        // Send response
        uint32_t resultSize = htonl(result.size());
        if (!sendAll(clientSocket, (char*)&resultSize, sizeof(resultSize)) ||
            !sendAll(clientSocket, result.c_str(), result.size())) {
            printError("[" + std::string(clientIP) + "] Failed to send complete result");
        }
        else {
            printSuccess("[" + std::string(clientIP) + "] Processed " + jobType);
        }
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
    WSADATA wsaData;
    SOCKET serverSocket;
    sockaddr_in serverAddr{};

    setColor(CYAN);
    std::cout << "Initializing Text Processing Manager...\n";

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printError("WSAStartup failed");
        return 1;
    }

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        printError("Socket creation failed");
        WSACleanup();
        return 1;
    }

    // Configure socket
    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
        printError("Setsockopt failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Bind socket
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(TEXT_MANAGER_PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        printError("Bind failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for connections
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        printError("Listen failed");
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    printSuccess("Text Processing Manager running on port " + std::to_string(TEXT_MANAGER_PORT));

    // Main server loop
    while (true) {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket == INVALID_SOCKET) {
            if (WSAGetLastError() != WSAEINTR) {
                printError("Accept failed");
            }
            continue;
        }

        // Handle client in a new thread
        std::thread([clientSocket]() {
            handleClient(clientSocket);
            }).detach();
    }

    // Cleanup (unreachable in this simple example)
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}