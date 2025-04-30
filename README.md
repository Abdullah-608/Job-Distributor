# Job Distributor System

#Youtube Video
https://youtu.be/ML4PzEhIASA
Full Explanation of What i made in this video !!!

## Overview
The Job Distributor is a distributed system designed to handle various computational tasks across multiple clients, servers, managers, and workers. It supports concurrent job submissions and provides real-time feedback to users. The system is modular and extensible, allowing for the addition of new job types in the future.

---

## System Components

### 1. **Client**
The Client is the entry point of the system, providing an interactive interface for users to submit jobs.

#### 🔹 Key Features:
- **Job Selection**: Users can choose from multiple job types (e.g., Strings, Images, Arrays).
- **Asynchronous Operation**: Handles multiple job requests simultaneously using multithreading.
- **User-Friendly Input**: Prompts users for relevant data based on the selected task.
- **Robust Communication**: Uses socket programming to communicate with the Server.
- **Real-Time Feedback**: Displays results with colored logs for better readability (success, error, status).
- **Flexible Design**: Easily extendable for new job types.

---

### 2. **Server**
The Server acts as the central hub, managing communication between Clients and Managers.

#### 🔹 Key Responsibilities:
- **Multi-Client Handling**: Processes multiple clients simultaneously using multithreading.
- **Job Routing**: Identifies the job type and forwards tasks to the appropriate Manager.
- **Result Relay**: Collects responses from Managers and sends them back to the correct Client.
- **Client Identification**: Maintains a mapping of client IDs for accurate response delivery.

---

### 3. **Manager**
The Manager is responsible for load balancing, client tracking, and job delegation.

#### 🔹 Key Responsibilities:
- **Client ID Tracking**: Logs which client requested which job for accurate response routing.
- **Load Management**: Balances job assignments across multiple Workers based on availability.
- **Task Delegation**: Breaks down jobs and forwards them to the appropriate Worker nodes.
- **Response Handling**: Collects processed results from Workers and sends them back to the Server.

---

### 4. **Worker**
Workers are specialized processing units optimized for specific tasks.

#### 🔹 Key Responsibilities:
- **Job Execution**: Executes the actual logic for the received data (e.g., Strings, Images, Arrays).
- **Thread Isolation**: Each Worker runs in its own thread to handle tasks concurrently.
- **Response Communication**: Sends processed results back to the Manager.

---

## Supported Job Types

### 📝 **Text Jobs**
Tasks utilizing Natural Language Processing (NLP) techniques.

| **Job**           | **Description**                                                                 | **Algorithm**                                                                 |
|--------------------|---------------------------------------------------------------------------------|--------------------------------------------------------------------------------|
| `text sentiment`   | Detects the sentiment (positive, negative, neutral) of the input text.          | Rule-based sentiment scoring or ML-based classification (e.g., Logistic Regression, BERT). |
| `text language`    | Identifies the language of the input text.                                      | Character-frequency analysis or libraries like `langdetect`.                  |
| `text spam`        | Classifies input as spam or not.                                                | Naive Bayes or keyword rule-based detection.                                  |
| `text summary`     | Generates a concise summary of the input text.                                  | Extractive summarization using TextRank or ML models.                         |
| `text entities`    | Extracts named entities (e.g., people, places, dates).                          | Named Entity Recognition (NER) via spaCy or transformer models.               |

---

### 🧮 **Array Jobs**
Numerical or algorithmic computations on arrays.

| **Job**             | **Description**                                      | **Algorithm**                                         |
|----------------------|------------------------------------------------------|------------------------------------------------------|
| `quicksort`          | Sorts an array efficiently.                          | Quick Sort (O(n log n)).                             |
| `median of median`   | Finds the median using the Median of Medians method. | Deterministic linear time selection (O(n)).          |
| `gcd`               | Calculates the Greatest Common Divisor of two numbers. | Euclidean algorithm.                                 |
| `kadane`             | Finds the maximum subarray sum.                      | Kadane’s Algorithm (O(n)).                           |
| `rotate`             | Rotates the array by k steps.                        | Reversal or cyclic replacement method.               |

---

### 🖼️ **Image Jobs**
Basic computer vision and image processing tasks.

| **Job**          | **Description**                              | **Algorithm**                                         |
|-------------------|----------------------------------------------|------------------------------------------------------|
| `image gray`      | Converts an image to grayscale.              | Weighted sum of RGB channels.                       |
| `image edge`      | Detects edges in the image.                  | Canny edge detection or Sobel filters.              |
| `image resize`    | Resizes the image to given dimensions.       | Bilinear or nearest-neighbor interpolation.          |
| `image rotate`    | Rotates the image by a specified angle.      | Affine transformation using a rotation matrix.       |
| `image blur`      | Applies a blur effect to the image.          | Gaussian blur or averaging kernel.                  |

---

## How to Run

### 1. Clone the Repository
```bash
git clone <repository-url>
cd job-distributor

###Future Enhancements
Add support for more job types (e.g., video processing, advanced ML tasks).
Implement a web-based client interface for easier interaction.
Enhance load balancing algorithms for better performance.
Introduce fault tolerance mechanisms for worker failures.
License
This project is licensed under the MIT License. See the LICENSE file for details. ```
