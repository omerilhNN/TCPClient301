#include <winsock2.h>
#include <windows.h>
#include <iostream>
#include <string>
#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 36
#define SERVER_IP "127.0.0.1"  // Sunucunun IP adresini buraya girin
#define BUFFER_SIZE 512

void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags);

struct ClientContext {
    SOCKET clientSock;
    char buffer[BUFFER_SIZE];
    WSABUF wsaBuf;
    OVERLAPPED overlapped;
    std::string message;
};

void sendMessage(ClientContext* context) {
    context->wsaBuf.buf = const_cast<char*>(context->message.c_str());
    context->wsaBuf.len = context->message.length();
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));

    int result = WSASend(context->clientSock, &context->wsaBuf, 1, NULL, 0, &context->overlapped, CompletionRoutine);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSASend failed with error: " << WSAGetLastError() << std::endl;
        closesocket(context->clientSock);
        delete context;
        WSACleanup();
        exit(1);
    }
}

void receiveMessage(ClientContext* context) {
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    context->wsaBuf.buf = context->buffer;
    context->wsaBuf.len = BUFFER_SIZE;

    int result = WSARecv(context->clientSock, &context->wsaBuf, 1, NULL, 0, &context->overlapped, CompletionRoutine);
    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        std::cerr << "WSARecv failed with error: " << WSAGetLastError() << std::endl;
        closesocket(context->clientSock);
        delete context;
        WSACleanup();
        exit(1);
    }
}

void CALLBACK CompletionRoutine(DWORD dwError, DWORD cbTransferred, LPWSAOVERLAPPED lpOverlapped, DWORD dwFlags) {
    ClientContext* context = (ClientContext*)lpOverlapped->hEvent;

    if (dwError != 0 || cbTransferred == 0) {
        std::cerr << "CompletionRoutine failed with error: " << dwError << std::endl;
        closesocket(context->clientSock);
        delete context;
        WSACleanup();
        exit(1);
    }

    if (cbTransferred > 0) {
        context->buffer[cbTransferred] = '\0';
        std::cout << "Received from server: " << context->buffer << std::endl;
    }

    std::string val1, val2, op;
    std::cout << "Enter val1: ";
    std::cin >> val1;
    std::cout << "Enter val2: ";
    std::cin >> val2;
    std::cout << "Enter operation (+, -, *, /): ";
    std::cin >> op;

    if (op != "+" && op != "-" && op != "*" && op != "/") {
        std::cerr << "Invalid operation. Try again." << std::endl;
        return;
    }

    context->message = val1 + "," + val2 + "," + op;
    sendMessage(context);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET clientSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = inet_addr(SERVER_IP);

    if (connect(clientSock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connect failed with error: " << WSAGetLastError() << std::endl;
        closesocket(clientSock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server at " << SERVER_IP << ":" << SERVER_PORT << std::endl;

    ClientContext* context = new ClientContext;
    context->clientSock = clientSock;
    ZeroMemory(&context->overlapped, sizeof(OVERLAPPED));
    context->overlapped.hEvent = context;

    receiveMessage(context);

    while (true) {
        SleepEx(INFINITE, TRUE);
    }

    closesocket(clientSock);
    WSACleanup();
    return 0;
}
