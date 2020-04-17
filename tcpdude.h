/**
 * Created by vava94 ( https://github.com/vava94 )
 * Source code ( https://github.com/vava94/TCPDude )
 *
 * A Class for TCP operations. Can run in server or client mode.
 *
 * --> Supports multiple connection.
 * --> Support UNIX and WIN32.
 *
 * Experimental:
 * --> Maybe can IPv6, not tested.
 *
 */

#pragma once
///**************************************************************************************
///--- Headers --------------------------------------------------------------------------
///**************************************************************************************
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
typedef unsigned long ulong;
#elif __linux
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int SOCKET;
#endif

class TCPDude {

private:
    #define MAX_READ_BYTES 81920
    /// Target socket class
    class TargetSocket {

    private:
        bool connected = true;
        SOCKET socketDescriptor;
        sockaddr_in socketAddress{};

    public:
        TargetSocket(SOCKET socketDescriptor, sockaddr_in socketAddress) {
            this->socketDescriptor = socketDescriptor;
            this->socketAddress = socketAddress;
        }

        inline void disconnect() { connected = false; }
        inline sockaddr_in* address() { return &socketAddress; }
        [[nodiscard]] inline SOCKET descriptor() const { return socketDescriptor; }
        std::thread *socketThread = nullptr;
        [[nodiscard]] inline bool isConnected() const { return connected; }
    };

    bool listenFlag = false;    /// Listening for new clients flag
    int mOperationMode = -1,    /// Operating mode
        mLastError = NO_ERRORS;
    SOCKET socketDescriptor = 0;   /// Server socket descriptor
    std::map<SOCKET, TargetSocket*> targets; /// Array of targets
    std::thread *listenThread{};  /// Listen for new clients thread

    std::function<void(SOCKET, char*, size_t)> mDataCallback = nullptr;
	std::function<void(SOCKET)> mConnectedCallback = nullptr;
	std::function<void(SOCKET)> mDisconnectedCallback = nullptr;
	std::function<void(int)> mErrorHandlerCallback = nullptr;
	std::function<void(TargetSocket *)> fReadLoop;
	std::function<void(SOCKET)> fListenLoop;

    void mDisconnected(SOCKET clientDescriptor);
    void mListenLoop(SOCKET serverDescriptor);
    void mNewTarget(SOCKET targetDescriptor, sockaddr_in targetAddress);
    void mReadLoop(TargetSocket *targetSocket);

public:
    /**
     * Operation modes.
     */
    enum OperationMode {
        SERVER_MODE,
        CLIENT_MODE
    };
    /**
     * Error Codes.
     */
    enum ErrorCode {
        NO_ERRORS,
        WRONG_OPERATION_MODE,
        SOCKET_CREATION_FAILED,
        SOCKET_CONNECT_FAILED,
        CLIENT_ACCEPT_FAILED,
        SOCKET_LISTEN_FAILED,
        SOCKET_BIND_FAILED
    };

    /**
     * Constructor
     * @param operationMode
     */
    explicit TCPDude(int operationMode);
    /**
     * Destructor
     */
    ~TCPDude();
    /**
     * Function for disconnect specific socket.
     * @param socketDescriptor
     */
    void disconnect(SOCKET socketDescriptor);
    /**
     * Function disconnecting all connections.
     */
    void disconnectAll();
    /**
     * Get socket address by descriptor.
     * @param descriptor
     * @return address string
     */
    std::string getAddress(SOCKET descriptor);
    /**
     * Get last error code.
     * @return error code
     */
    int getLastError() const;
    /**
     * @return current operation mode;
     */
    int getOperationMode() const;
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
    /**
     * Sending function.
     * Disconnect socket on error transmitting.
     * @param socketDescriptor
     * @param data
     * @param size
     * @return true on success sending, false in failure.
     */
    bool send(SOCKET socketDescriptor, char *data, ulong size);
    /// Servers function
    /**
     * Launch server function.
     * @param port
     * @return true on success launch
     */
    bool startServer(uint16_t port);
    /**
     * Stopping server function.
     */
    void stopServer();  // Функция остановки сервера
    /**
     * Function for connecting client to server.
     * @param address
     * @param port
     * @return servers socket descriptor on success.
     */
    SOCKET clientConnectToServer(std::string address, unsigned short port);

};