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

queue<HANDLE> eventQueue;
mutex queueMutex;
vector<thread> socketThreads;
const char* SERVER_ADDRESS = "192.168.254.16";
const int SERVER_PORT = 36;

// SocketHandler thread function
DWORD WINAPI SocketHandler(LPVOID lpParam) {
    SOCKET clientSocket = *reinterpret_cast<SOCKET*>(lpParam);

    WSAEVENT wsaEvent = WSACreateEvent();
    if (WSAEventSelect(clientSocket, wsaEvent, FD_CONNECT | FD_CLOSE) == SOCKET_ERROR) {
        cerr << "WSAEventSelect failed with error: " << WSAGetLastError() << endl;
        closesocket(clientSocket);
        return 1;
    }
    cout << "Attempting to connect " << endl;
    HANDLE events[1] = { wsaEvent };

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDRESS, &server_addr.sin_addr) <= 0) {
        cout << "Invalid IP" << endl;
        return 1;
    }

    if (connect(clientSocket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
            cerr << "Connect failed with error: " << WSAGetLastError() << endl;
            closesocket(clientSocket);
            return 1;
        }
    }

    while (true) {
        DWORD waitResult = WSAWaitForMultipleEvents(1, events, FALSE, INFINITE, TRUE);
        if (waitResult == WSA_WAIT_FAILED) {
            cerr << "WSAWaitForMultipleEvents Failed: " << WSAGetLastError() << endl;
            break;
        }
        if (waitResult == WAIT_OBJECT_0) {
            queueMutex.lock();
            SetEvent(wsaEvent);
            eventQueue.push(wsaEvent);
            queueMutex.unlock();
            break;
        }
    }

    return 0;
}

// Manager thread function
DWORD WINAPI Manager(LPVOID lpParam) {
    SOCKET clientSocket = *reinterpret_cast<SOCKET*>(lpParam);

    while (true) {
        queueMutex.lock();
        if (!eventQueue.empty()) {
            HANDLE sockEvent = eventQueue.front();
            eventQueue.pop();
            queueMutex.unlock();

            WSANETWORKEVENTS netEvents;
            if (WSAEnumNetworkEvents(clientSocket, sockEvent, &netEvents) == SOCKET_ERROR) {
                cerr << "WSAEnumNetworkEvents failed with error: " << WSAGetLastError() << endl;
                continue;
            }

            if (netEvents.lNetworkEvents & FD_CONNECT) {
                cout << "Connected to server successfully " << endl;
                //TODO: Client Processes after connected to server successfully
            }

            if (netEvents.lNetworkEvents & FD_CLOSE) {
                cout << "Server shutdown" << endl;
                closesocket(clientSocket);
                break;
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

    HANDLE hSocketHandlerThread = CreateThread(NULL, 0, SocketHandler, reinterpret_cast<LPVOID>(&clientSocket), 0, NULL);
    HANDLE hManagerThread = CreateThread(NULL, 0, Manager, reinterpret_cast<LPVOID>(&clientSocket), 0, NULL);

    WaitForSingleObject(hSocketHandlerThread, INFINITE);
    WaitForSingleObject(hManagerThread, INFINITE);

    CloseHandle(hManagerThread);
    CloseHandle(hSocketHandlerThread);

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
