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
    int mOperationMode = -1,    /// Operating mode
        mLastError = NO_ERRORS;
    SOCKET socketDescriptor = 0;   /// Server socket descriptor
    std::map<SOCKET, TargetSocket*> targets; /// Array of targets
    std::thread *listenThread;  /// Listen for new clients thread

    std::function<void(SOCKET, char*, size_t)> dataCallback = nullptr;
	std::function<void(SOCKET)> mConnectedCallback = nullptr;
	std::function<void(SOCKET)> mDisconnectedCallback = nullptr;
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
        NO_ERRORS,
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
    std::string getAddress(SOCKET descriptor);
    int getLastError();
    /**
     * @return current operation mode;
     */
    int getOperationMode();
    /**
     * Search connected socket descriptor by address and port.
     * @param address
     * @param port
     * @return socket descriptor
     */
    SOCKET getSocketDescriptor(const std::string& address, uint16_t port);
    /**
     * Callback setup function for a signal about new incoming data.
     * @param callback void(SOCKET descriptor, char *data, size_t dataSize)
     */
    void setDataReadyCallback(std::function<void(SOCKET, char*, size_t)> DataCallback);
    /**
     * Callback setup function for a signal about a new client connection to the server in server mode or when client
     * connected to server in client mode.
     * @param callback void(SOCKET socketDescriptor) SOCKET = int in UNIX.
     */
    void setConnectedCallback(std::function<void(SOCKET socketDescriptor)> callback);
    /**
     * Callback setup function for a signal about a client disconnection from the server in server mode or when client
     * disconnected from server in client mode.
     * @param callback void(SOCKET socketDescriptor) SOCKET = int in UNIX.
     */
    void setDisconnectedCallback(std::function<void(SOCKET socketDescriptor)> callback);
    /**
     * Callback setup function for errors.
     * @param callback void(int errorCode)
     */
    void setErrorHandlerCallback(std::function<void(int)> callback);
    bool send(SOCKET socketDescriptor, char *data, ulong size);
    //--- Сервер ------------------------------------------------------------------------
    bool startServer(uint16_t port); // Функция запуска сервера
    void stopServer();  // Функция остановки сервера
    //--- Клиент ------------------------------------------------------------------------
    // Функция подключения клиента к серверу
    SOCKET clientConnectToServer(std::string address, unsigned short port);
    void disconnectFromServer(SOCKET socketDescriptor);
};