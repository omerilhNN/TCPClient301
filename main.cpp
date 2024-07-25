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

//struct SocketEvent {
//    SOCKET socket;
//    HANDLE event;
//};

queue<HANDLE> eventQueue;
mutex queueMutex;
vector<thread> socketThreads;
const char* SERVER_ADDRESS = "192.168.254.16";
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
    if (WSAEventSelect(clientSocket, wsaEvent, FD_CONNECT | FD_CLOSE) == SOCKET_ERROR) {
        cerr << "WSAEventSelect failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        CloseHandle(hEvent);
        return 1;
    }

    HANDLE events[2] = { hEvent, wsaEvent };

    while (true) {
        DWORD waitResult = WaitForMultipleObjectsEx(2, events, FALSE, INFINITE,TRUE);

        if (waitResult == WAIT_OBJECT_0) {
            // Custom event triggered
            queueMutex.lock();
            eventQueue.push( hEvent);
            queueMutex.unlock();
            ResetEvent(hEvent);
        }
        else if (waitResult == WAIT_OBJECT_0 + 1) {
            queueMutex.lock();
            eventQueue.push(wsaEvent);
            queueMutex.unlock();
        }
    }

    closesocket(clientSocket);
    CloseHandle(hEvent);
    WSACloseEvent(wsaEvent);
    return 0;
}

DWORD WINAPI Manager(LPVOID lpParam) {
    SOCKET clientSocket = reinterpret_cast<SOCKET>(lpParam);
    sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        cout << "Invalid IP" << endl;
        return 1;
    }

    while (true) {
        queueMutex.lock();
        if (!eventQueue.empty()) {
            HANDLE sockEvent = eventQueue.front();
            eventQueue.pop();
            queueMutex.unlock();

            // Process the event based on the socket event
            WSANETWORKEVENTS netEvents;
            if (WSAEnumNetworkEvents(clientSocket, sockEvent, &netEvents) == SOCKET_ERROR) {
                cerr << "WSAEnumNetworkEvents failed with error: " << WSAGetLastError() << endl;
                continue;
            }

            if (netEvents.lNetworkEvents & FD_CONNECT) {
                int connResult= connect(clientSocket, (sockaddr*)&server_addr, sizeof(server_addr));
                if (connResult == SOCKET_ERROR) {
                    cerr << "Can't connect to server! Quitting" << endl;
                    closesocket(clientSocket);
                    WSACleanup();
                    exit(1);
                }
                else {
                    cerr << "Connected to server succesfully " << endl;
                }
            }

            if (netEvents.lNetworkEvents & FD_CLOSE) {
                cout << "Server disconnected" << endl;
                closesocket(clientSocket);
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
    int wsa = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsa != 0) {
        cerr << "WSAStartup failed: " << wsa << endl;
        exit(1);
    }
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        cerr << "Can't create a socket! Quitting" << endl;
        WSACleanup();
        exit(1);
    }
    HANDLE hSocketHandlerThread = CreateThread(NULL, 0, SocketHandler, reinterpret_cast<LPVOID>(clientSocket), 0, NULL);
    HANDLE hManagerThread = CreateThread(NULL, 0, Manager, reinterpret_cast<LPVOID>(clientSocket), 0, NULL);

    const char* testMessage = "Hello from the client!";
    send(clientSocket, testMessage, strlen(testMessage), 0);

    WaitForSingleObject(hManagerThread, INFINITE);
    CloseHandle(hManagerThread);
    CloseHandle(hSocketHandlerThread);

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
