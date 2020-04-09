/**
 * Created by vava94 ( https://github.com/vava94 )
 * Source code ( https://github.com/vava94/TCPDude )
 *
 * A Class for TCP operations. Can run in server or client mode. Server also supports
 * multi-client mode.
 *
 * Supports UNIX and WIN32
 */

#pragma once
///**************************************************************************************
///--- Headers --------------------------------------------------------------------------
///**************************************************************************************
#include <functional>

#include <thread>
#include <vector>
#include <map>
#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
#elif __linux
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#endif

///**************************************************************************************
///--- Класс, осуществляющий приём и передачу данных по протоколу TCP. Может работать ---
///--- как в режиме есервера, так и в режиме клиента. -----------------------------------
///**************************************************************************************
class TCPDude {

private:
    #define MAX_READ_BYTES 81920
    /// Target socket class
    class TargetSocket {

    private:
        bool connected = true;
        SOCKET socketDescriptor;
        sockaddr_in socketAddress;

    public:
        TargetSocket(SOCKET socketDescriptor, sockaddr_in socketAddress) {
            this->socketDescriptor = socketDescriptor;
            this->socketAddress = socketAddress;
        }

        inline void disconnect() { connected = false; }
        inline sockaddr_in* address() { return &socketAddress; }
        inline SOCKET descriptor() { return socketDescriptor; }
        std::thread *socketThread = nullptr;
        inline bool isConnected() { return connected; }
    };

    bool listenFlag = false;    /// Listening for new clients flag
    int operationMode = -1;     /// Operating mode
    SOCKET socketDescriptor = 0;   /// Server socket descriptor
    std::map<SOCKET, TargetSocket*> targets; /// Array of targets
    std::thread *listenThread;  /// Listen for new clients thread

    std::function<void(std::string, char*, ulong)> dataCallback = nullptr;
	std::function<void(SOCKET)> clientConnectedCallback = nullptr;
	std::function<void(SOCKET)> clientDisconnectedCallback = nullptr;
	std::function<void(int)> ErrorHandlerCallback = nullptr;
	std::function<void(TargetSocket *)> fReadLoop;
	std::function<void(SOCKET)> fListenLoop;

    void readLoop(TargetSocket *targetSocket); // Цикл приёма данных

    void clientDisconnected(SOCKET clientDescriptor);  //Обработчик отключения клиента
    void mListenLoop(SOCKET serverDescriptor); // Цикл ожидания подключения клиентов
    // Функция обработки нового сокета
    void mNewTarget(SOCKET targetDescriptor, sockaddr_in targetAddress);

public:
    // Перечисление режимов работы
    enum OperationMode {
        SERVER_MODE,
        CLIENT_MODE
    };
    enum ErrorCode {
        WRONG_OPERATION_MODE,
        SOCKET_CREATION_FAILED,
        SOCKET_CONNECT_FAILED,
        CLIENT_ACCEPT_FAILED,
        SOCKET_LISTEN_FAILED,
        SOCKET_BIND_FAILED
    };

    // Конструктор
    explicit TCPDude(int operationMode);
    ~TCPDude(); //Деструктор
    int getOperationMode(); // Возвращает режим работы
    SOCKET getSocketDescriptor(const std::string& address, uint16_t port);
    void setDataReadyCallback(std::function<void(std::string, char*, ulong)> DataCallback);
    void setClientConnectedCallback(std::function<void(SOCKET socketDescriptor)> connectedCallbackFunc);
    void setClientDisconnectedCallback(std::function<void(SOCKET socketDescriptor)> disConnectedCallbackFunc);
    void setErrorHandlerCallback(std::function<void(int)> ErrorHandlerCallback);
    void send(SOCKET socketDescriptor, char *data, ulong size);
    //--- Сервер ------------------------------------------------------------------------
    void startServer(uint16_t port); // Функция запуска сервера
    void stopServer();  // Функция остановки сервера
    //--- Клиент ------------------------------------------------------------------------
    // Функция подключения клиента к серверу
    SOCKET clientConnectToServer(std::string address, unsigned short port);
    void disconnectFromServer(SOCKET socketDescriptor);
};