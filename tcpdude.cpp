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

///**************************************************************************************
///--- Headers --------------------------------------------------------------------------
///**************************************************************************************
///-- Local
#include "tcpdude.h"

///-- C++
#include <memory.h>

/**
 * Constructor
 * @param operationMode
 */
TCPDude::TCPDude(int operationMode) {
    mOperationMode = operationMode;
    fReadLoop = bind(&TCPDude::mReadLoop, this, std::placeholders::_1);
    fListenLoop = bind(&TCPDude::mListenLoop, this, std::placeholders::_1);
}
/**
 * The function connects the client to the server.
 * @param address of server
 * @param port of server
 * @return server's socket descriptor on success and -1 on fail.
 */
SOCKET TCPDude::clientConnectToServer(std::string address, unsigned short port) {
    if(mOperationMode == SERVER_MODE) return ErrorCode::WRONG_OPERATION_MODE;
    sockaddr_in _targetAddress{};
    SOCKET _targetDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(_targetDescriptor == -1) {
        mErrorHandlerCallback(ErrorCode::SOCKET_CREATION_FAILED);
        return -1;
    }
    memset(&_targetAddress, 0, sizeof (_targetAddress));
    _targetAddress.sin_family = AF_INET;
    _targetAddress.sin_port = htons(port);
    if(inet_pton(AF_INET, address.data(), &_targetAddress.sin_addr) <= 0)
        return -1;

    if (connect(_targetDescriptor, reinterpret_cast<sockaddr *>(&_targetAddress),
                       sizeof(_targetAddress)) < 0) {
        if(mErrorHandlerCallback) {
            mErrorHandlerCallback(ErrorCode::SOCKET_CONNECT_FAILED);
        }
        return -1;
    }
    mNewTarget(_targetDescriptor, _targetAddress);
    if(mConnectedCallback) {
        mConnectedCallback(_targetDescriptor);
    }
    return _targetDescriptor;
}


void TCPDude::mDisconnected(SOCKET descriptor) {
    if(mDisconnectedCallback) {
        mDisconnectedCallback(descriptor);
    }
    auto _t = targets[descriptor];
    delete _t;
    targets.erase(descriptor);
}

void TCPDude::disconnectAll() {
    for(auto _target : targets) {
        disconnect(_target.first);
    }
}

void TCPDude::disconnect(SOCKET targetDescriptor) {
    targets[targetDescriptor]->disconnect();
    std::this_thread::sleep_for(std::chrono::microseconds (100));
#ifdef _WIN32
	shutdown(targetDescriptor, SD_BOTH);
	closesocket(targetDescriptor);
#elif __linux
	shutdown(targetDescriptor, SHUT_RDWR);
	close(targetDescriptor);
#endif

}

int TCPDude::getLastError() const {
    return mLastError;
}

std::string TCPDude::getAddress(SOCKET descriptor) {
    return std::string(inet_ntoa(targets[descriptor]->address()->sin_addr));
}

/**
 *
 * @return operation mode
 */
int TCPDude::getOperationMode() const {
    return mOperationMode;
}

/**
 * Find descriptor by address and port
 * @param address
 * @param port
 * @return socket descriptor
 */
SOCKET TCPDude::getSocketDescriptor(const std::string& address, uint16_t port) {

    SOCKET _descriptor = -1;
	std::string _address;

	for (auto _target : targets) {
        _address = std::move(std::string(inet_ntoa(_target.second->address()->sin_addr)));
        if((_address == address) && (_target.second->address()->sin_port == htons(port))){
            _descriptor = _target.first;
            break;
        }
    }
    return _descriptor;
}

/**
 * Server loop to wait for new clients to connect.
 * @param serverDescriptor
 */
void TCPDude::mListenLoop(SOCKET serverDescriptor) {
    while (listenFlag) {
        sockaddr_in _clientAddress {};
        int _len = sizeof (_clientAddress);
        SOCKET _clientDescriptor = accept(serverDescriptor,
                                       reinterpret_cast<sockaddr*>(&_clientAddress),
                                       reinterpret_cast<socklen_t*>(&_len));
        if(_clientDescriptor == -1 && listenFlag) {
            mErrorHandlerCallback(ErrorCode::CLIENT_ACCEPT_FAILED);
        } else {
            mNewTarget(_clientDescriptor, _clientAddress);
        }
    }
}

/**
 * Private func for handle new target sockets.
 * @param targetDescriptor
 * @param targetAddress
 */
void TCPDude::mNewTarget(SOCKET targetDescriptor, sockaddr_in targetAddress) {

    auto _targetSocket = new TargetSocket(targetDescriptor, targetAddress);
    targets.emplace(targetDescriptor, _targetSocket);
    if(mConnectedCallback) {
        mConnectedCallback(targetDescriptor);
    }

}

/**
 * Cycle for reading data from clients.
 * @param targetSocket for reading from.
 */
void TCPDude::mReadLoop(TargetSocket *targetSocket){

    char *_receiveBuffer = new char[MAX_READ_BYTES]();
    size_t _bytesRead;
    auto _descriptor = targetSocket->descriptor();

    while (targetSocket->isConnected()) {
        _bytesRead = recv(targetSocket->descriptor(), _receiveBuffer, MAX_READ_BYTES, 0);

        if(!targetSocket->isConnected())
            break;
        if(_bytesRead < 1) {
            targetSocket->disconnect();
        } else {
            mDataCallback(_descriptor, _receiveBuffer, _bytesRead);
        }
    }

    delete[] _receiveBuffer;
    mDisconnected(targetSocket->descriptor());

}

/**
 * Callback setting function for received data.
 * @param callback
 */
void TCPDude::setDataReadyCallback(std::function<void (SOCKET, char *, size_t)> callback) {
    mDataCallback = std::move(callback);
}

void TCPDude::setErrorHandlerCallback(std::function<void (int)> Callback) {
    mErrorHandlerCallback = std::move(Callback);
}

/**
 * Callback setup function for a signal about a new connection to the server.
 * @param callback
 */
void TCPDude::setConnectedCallback(std::function<void (SOCKET)> callback) {
    mConnectedCallback = std::move(callback);
}


void TCPDude::setDisconnectedCallback(std::function<void (SOCKET)> callback) {
    mDisconnectedCallback = std::move(callback);
}


bool TCPDude::startServer(uint16_t port) {
    if(mOperationMode == CLIENT_MODE) {
        mLastError = WRONG_OPERATION_MODE;
        return false;
    }

    sockaddr_in _serverAddress {};
    socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(socketDescriptor == -1) {
        mLastError = SOCKET_CREATION_FAILED;
        return false;
    }
    memset(&_serverAddress, 0, sizeof (_serverAddress));
    _serverAddress.sin_family = AF_INET;
    _serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    _serverAddress.sin_port = htons(port);
    if(::bind(socketDescriptor, reinterpret_cast<sockaddr*>(&_serverAddress),
            sizeof (_serverAddress)) != 0) {

#ifdef _WIN32
		shutdown(socketDescriptor, SD_BOTH);
		closesocket(socketDescriptor);
#elif __linux
		shutdown(socketDescriptor, SHUT_RDWR);
		close(socketDescriptor);
#endif
        mLastError = SOCKET_BIND_FAILED;
        return false;
    }
    if((listen(socketDescriptor, 3)) < 0) {

#ifdef _WIN32
		shutdown(socketDescriptor, SD_BOTH);
		closesocket(socketDescriptor);
#elif __linux
		shutdown(socketDescriptor, SHUT_RDWR);
		close(socketDescriptor);
#endif
        mLastError = SOCKET_LISTEN_FAILED;
        return false;
    }
    listenFlag = true;
    listenThread = new std::thread(fListenLoop, socketDescriptor);
    return true;
}

/**
 * Function stops server.
 */
void TCPDude::stopServer() {
    if(mOperationMode == CLIENT_MODE) return;
    listenFlag = false;
    if (listenThread != nullptr) {
        listenThread->join();
    }
}

/**
 * Function for sending
 * @param targetDescriptor
 * @param data
 * @param size
 * @return true on success
 */
bool TCPDude::send(SOCKET targetDescriptor, char *data, ulong size){

	if(::send(socketDescriptor, data, size, 0) < 1) {
        disconnect(targetDescriptor);
        return false;
	}
    return true;
}

/**
 * Destructor
 */
TCPDude::~TCPDude() {
    mDataCallback = nullptr;
    mConnectedCallback = nullptr;
    mDisconnectedCallback = nullptr;
    mErrorHandlerCallback = nullptr;
    if(mOperationMode == SERVER_MODE) {
        stopServer();
    }
    disconnectAll();
}
