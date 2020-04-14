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
    fReadLoop = bind(&TCPDude::readLoop, this, std::placeholders::_1);
    fListenLoop = std::bind(&TCPDude::mListenLoop, this, std::placeholders::_1);
}

/**
 * Cycle for reading data from clients.
 * @param targetSocket for reading from.
 */
void TCPDude::readLoop(TargetSocket *targetSocket){

    char *_receiveBuffer = static_cast<char*>(malloc(MAX_READ_BYTES));
    size_t _bytesRead = 0;
    auto _descriptor = targetSocket->descriptor();

    while (targetSocket->isConnected()) {
		_bytesRead = recv(targetSocket->descriptor(), _receiveBuffer, MAX_READ_BYTES, 0);

        if(!targetSocket->isConnected())
            break;
        if(_bytesRead < 1) {
            targetSocket->disconnect();
        } else {
            dataCallback(_descriptor, _receiveBuffer, _bytesRead);
        }
    }
    clientDisconnected(targetSocket->descriptor());

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
        ErrorHandlerCallback(ErrorCode::SOCKET_CREATION_FAILED);
        return -1;
    }
    memset(&_targetAddress, 0, sizeof (_targetAddress));
    _targetAddress.sin_family = AF_INET;
    _targetAddress.sin_port = htons(port);

    if(inet_pton(AF_INET, address.data(), &_targetAddress.sin_addr) <= 0)
        return -1;

    if (connect(_targetDescriptor, reinterpret_cast<sockaddr *>(&_targetAddress),
                       sizeof(_targetAddress)) < 0) {
        if(ErrorHandlerCallback) {
            ErrorHandlerCallback(ErrorCode::SOCKET_CONNECT_FAILED);
        }
        return -1;
    }
    mNewTarget(_targetDescriptor, _targetAddress);
    if(mConnectedCallback) {
        mConnectedCallback(_targetDescriptor);
    }
    return _targetDescriptor;
}

//***************************************************************************************
//--- Функция обработки отключения клиента ----------------------------------------------
//***************************************************************************************
void TCPDude::clientDisconnected(SOCKET clientDescriptor) {
    if(mDisconnectedCallback) {
        mDisconnectedCallback(clientDescriptor);
    }
    targets.erase(clientDescriptor);
}

//***************************************************************************************
//--- Функция отключения клиента от сервера ---------------------------------------------
//***************************************************************************************
void TCPDude::disconnectFromServer(SOCKET targetDescriptor) {
    if(mOperationMode == SERVER_MODE) return;
    targets[targetDescriptor]->disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
#ifdef _WIN32
	shutdown(targetDescriptor, SD_BOTH);
	closesocket(targetDescriptor);
#elif __linux
	shutdown(targetDescriptor, SHUT_RDWR);
	close(targetDescriptor);
#endif

}

int TCPDude::getLastError() {
    return mLastError;
}

std::string TCPDude::getAddress(SOCKET descriptor) {
    SOCKET _descriptor = -1;
    sockaddr *_sockaddr;
    std::string _address;
    char _s[INET_ADDRSTRLEN];
    char _s6[INET6_ADDRSTRLEN];
    auto _target = targets[descriptor];
    switch (_target->address()->sin_family) {
        //switch (reinterpret_cast<sockaddr *>(_target.second->address())->sa_family) {
        case AF_INET:
            _sockaddr = reinterpret_cast<sockaddr *>((struct sockaddr_in *) _target->address());
            inet_ntop(AF_INET, _sockaddr, _s, sizeof _s);
            _address = std::move(std::string(_s));
            break;

        case AF_INET6:
            _sockaddr = reinterpret_cast<sockaddr *>((struct sockaddr_in6 *) _target->address());
            inet_ntop(AF_INET6, _sockaddr, _s6, sizeof _s6);
            _address = std::move(std::string(_s6));
            break;

    }
    return _address;
}

/**
 *
 * @return operation mode
 */
int TCPDude::getOperationMode() {
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
    sockaddr *_sockaddr;
	std::string _address;
    char _s[INET_ADDRSTRLEN];
    char _s6[INET6_ADDRSTRLEN];

	for (auto _target : targets) {
	    switch (_target.second->address()->sin_family) {
            //switch (reinterpret_cast<sockaddr *>(_target.second->address())->sa_family) {
            case AF_INET:
                _sockaddr = reinterpret_cast<sockaddr *>((struct sockaddr_in *) _target.second->address());
                inet_ntop(AF_INET, _sockaddr, _s, sizeof _s);
                _address = std::move(std::string(_s));
                break;

            case AF_INET6:
                _sockaddr = reinterpret_cast<sockaddr *>((struct sockaddr_in6 *) _target.second->address());
                inet_ntop(AF_INET6, _sockaddr, _s6, sizeof _s6);
                _address = std::move(std::string(_s6));
                break;

        }

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
            ErrorHandlerCallback(ErrorCode::CLIENT_ACCEPT_FAILED);
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
 * Callback setting function for received data.
 * @param callback
 */
void TCPDude::setDataReadyCallback(std::function<void (SOCKET, char *, size_t)> callback) {
    dataCallback = std::move(callback);
}

//***************************************************************************************
//--- Функция установки обратного вызова для ошибок -------------------------------------
//***************************************************************************************
void TCPDude::setErrorHandlerCallback(std::function<void (int)> Callback) {
    ErrorHandlerCallback = std::move(Callback);
}

/**
 * Callback setup function for a signal about a new connection to the server.
 * @param callback
 */
void TCPDude::setConnectedCallback(std::function<void (SOCKET)> callback) {
    mConnectedCallback = std::move(callback);
}

/**
 * Callback setup function for a signal about a disconnection from the server.
 * @param callback
 */
void TCPDude::setDisconnectedCallback(std::function<void (SOCKET)> callback) {
    mDisconnectedCallback = std::move(callback);
}

//***************************************************************************************
//--- Функция запуска сервера -----------------------------------------------------------
//***************************************************************************************
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

//***************************************************************************************
//--- Функция остановки сервера ---------------------------------------------------------
//***************************************************************************************
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
	    switch (mOperationMode){
            case CLIENT_MODE:
                disconnectFromServer(targetDescriptor);
                break;
            case SERVER_MODE:
                clientDisconnected(targetDescriptor);
                break;
	    }
        return false;
	}
    return true;
}

//***************************************************************************************
//--- Деструктор ------------------------------------------------------------------------
//***************************************************************************************
TCPDude::~TCPDude() {

}
