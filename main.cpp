#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

struct SocketEvent {
    SOCKET socket;
    HANDLE event;
};

queue<SocketEvent> eventQueue;
mutex queueMutex;
vector<thread> socketThreads;
const char* SERVER_ADDRESS = "127.0.0.1";
const int SERVER_PORT = 36;

// SocketHandler thread function
DWORD WINAPI SocketHandler(LPVOID lpParam) {
    SOCKET clientSocket = reinterpret_cast<SOCKET>(lpParam);
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (hEvent == NULL) {
        cerr << "CreateEvent failed with error: " << GetLastError() << endl;
        closesocket(clientSocket);
        return 1;
    }

    WSAEVENT wsaEvent = WSACreateEvent();
    if (WSAEventSelect(clientSocket, wsaEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
        cerr << "WSAEventSelect failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        CloseHandle(hEvent);
        return 1;
    }

    HANDLE events[2] = { hEvent, wsaEvent };

    while (true) {
        DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            // Custom event triggered
            queueMutex.lock();
            eventQueue.push({ clientSocket, hEvent });
            queueMutex.unlock();
            ResetEvent(hEvent);
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            queueMutex.lock();
            eventQueue.push({ clientSocket, wsaEvent });
            queueMutex.unlock();
        }
    }

    closesocket(clientSocket);
    CloseHandle(hEvent);
    WSACloseEvent(wsaEvent);
    return 0;
}

// Manager thread function
DWORD WINAPI Manager(LPVOID lpParam) {
    while (true) {
        queueMutex.lock();
        if (!eventQueue.empty()) {
            SocketEvent sockEvent = eventQueue.front();
            eventQueue.pop();
            queueMutex.unlock();

            // Process the event based on the socket event
            WSANETWORKEVENTS netEvents;
            if (WSAEnumNetworkEvents(sockEvent.socket, sockEvent.event, &netEvents) == SOCKET_ERROR) {
                cerr << "WSAEnumNetworkEvents failed with error: " << WSAGetLastError() << endl;
                continue;
            }

            if (netEvents.lNetworkEvents & FD_READ) {
                char buf[4096];
                int bytesReceived = recv(sockEvent.socket, buf, 4096, 0);
                if (bytesReceived > 0) {
                    string receivedData(buf, 0, bytesReceived);
                    cout << "Received: " << receivedData << endl;
                }
                else {
                    cerr << "recv failed with error: " << WSAGetLastError() << endl;
                }
            }

            if (netEvents.lNetworkEvents & FD_CLOSE) {
                cout << "Server disconnected" << endl;
                closesocket(sockEvent.socket);
            }
        }
        else {
            queueMutex.unlock();
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    return 0;
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    sockaddr_in serverHint;

    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        cerr << "WSAStartup failed: " << iResult << endl;
        exit(1);
    }
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Can't create a socket! Quitting" << endl;
        WSACleanup();
        exit(1);
    }

    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &serverHint.sin_addr) <= 0) {
        cout << "Invalid IP" << endl;
        return 1;
    }

    iResult = connect(clientSocket, (sockaddr*)&serverHint, sizeof(serverHint));
    if (iResult == SOCKET_ERROR) {
        cerr << "Can't connect to server! Quitting" << endl;
        closesocket(clientSocket);
        WSACleanup();
        exit(1);
    }

    // Create and start the SocketHandler thread
    HANDLE hSocketHandlerThread = CreateThread(NULL, 0, SocketHandler, reinterpret_cast<LPVOID>(clientSocket), 0, NULL);

    // Create and start the Manager thread
    HANDLE hManagerThread = CreateThread(NULL, 0, Manager, NULL, 0, NULL);

    // Send a test message to the server
    const char* testMessage = "Hello from the client!";
    send(clientSocket, testMessage, strlen(testMessage), 0);

    // Wait for the Manager thread to finish (in practice, you may want to implement a graceful shutdown mechanism)
    WaitForSingleObject(hManagerThread, INFINITE);
    CloseHandle(hManagerThread);
    CloseHandle(hSocketHandlerThread);

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
