#include <iostream>
#include <string>
#include <winsock2.h>
#include <windows.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define IP "192.168.254.21"

void* SendDataToServer(const std::string& data) {
    HANDLE hPipe = CreateFile(
        TEXT("\\\\.\\pipe\\MyPipe"),
        GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hPipe != INVALID_HANDLE_VALUE) {
        DWORD dwWritten;
        WriteFile(hPipe, data.c_str(), data.size(), &dwWritten, NULL);
        CloseHandle(hPipe);
    }
    else {
        std::cerr << "Failed to connect to the pipe." << std::endl;
    }
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    sockaddr_in serverAddr;
    std::string val1, val2, op;

    WSAStartup(MAKEWORD(2, 2), &wsaData);
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(36);
    if (inet_pton(AF_INET,IP,&serverAddr.sin_addr) <= 0) {
        printf("\nInvalid address");
        return 1;
    }

    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::cout << "Enter val1: ";
    std::cin >> val1;
    std::cout << "Enter val2: ";
    std::cin >> val2;
    std::cout << "Enter operation (e.g., +, -, *, /): ";
    std::cin >> op;

    std::string message = val1 + " " + val2 + " " + op;
    SendDataToServer(message);

    char buffer[256];
    int bytesReceived = recv(clientSocket, buffer, 256, 0);
    if (bytesReceived > 0) {
        std::cout << "Result from server: " << std::string(buffer, 0, bytesReceived) << std::endl;
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}
