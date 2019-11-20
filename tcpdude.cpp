//***************************************************************************************
//--- Заголовочные ----------------------------------------------------------------------
//***************************************************************************************
//-- Локальные
#include "../shared_sources/errors.h"
#include "tcpdude.h"


//***************************************************************************************
//--- Private static variables ----------------------------------------------------------
//***************************************************************************************

#define MAX_READ_BYTES 81920

//***************************************************************************************
//--- Внешнии функции main --------------------------------------------------------------
//***************************************************************************************
void ClientDisconnected(int socketDescriptor);

//***************************************************************************************
//--- Конструктор -----------------------------------------------------------------------
//***************************************************************************************
TCPDude::TCPDude(int operationMode) {
    this->operationMode = operationMode;
    targetSockets = reinterpret_cast<TargetSocket*>(malloc(sizeof (TargetSocket)));
    fReadLoop = bind(&TCPDude::ReadLoop, this, std::placeholders::_1);
    fListenLoop = bind(&TCPDude::ListenLoop, this, std::placeholders::_1);
}

//***************************************************************************************
//--- Цикл приёма данных ----------------------------------------------------------------
//***************************************************************************************
void TCPDude::ReadLoop(void* arg){
    TargetSocket *_targetSocket = reinterpret_cast<TargetSocket*>(arg);
    string _address = "";
    switch (reinterpret_cast<sockaddr *>(_targetSocket->AddressPtr())->sa_family) {
        case AF_INET: {
            char _addr[16];
            inet_ntop(AF_INET, &(_targetSocket->AddressPtr()->sin_addr),
                      _addr, sizeof (_addr));
            _address = _addr;
            break;
        }

        case AF_INET6: {
            char _addr[39];
            inet_ntop(AF_INET6, &(_targetSocket->AddressPtr()->sin_addr),
                      _addr, sizeof (_addr));
            _address = _addr;
            break;
        }
    }
    uchar *_receiveBuffer = static_cast<uchar*>(malloc(MAX_READ_BYTES));
    long _bytesRead = 0;

    while (_targetSocket->IsConnected()) {
        memset(_receiveBuffer, 0, MAX_READ_BYTES);
#ifdef _WIN32
		_bytesRead = recv(_targetSocket->Descriptor(), 
			reinterpret_cast<char*>(_receiveBuffer), MAX_READ_BYTES, 0);
#elif __linux
		_bytesRead = recv(_targetSocket->Descriptor(), _receiveBuffer, MAX_READ_BYTES, 0);
#endif
        if(!_targetSocket->IsConnected())
            break;
        if(_bytesRead == 0) {
            _targetSocket->Disconnect();
        } else {
            DataCallback(_address, _receiveBuffer, static_cast<ulong>(_bytesRead));
        }
    }
    ClientDisconnected(_targetSocket->Descriptor());
}

//***************************************************************************************
//--- Функция подключения клиента к серверу ---------------------------------------------
//***************************************************************************************
SOCKET TCPDude::ClientConnectToServer(string address, unsigned short port) {
    if(operationMode == SERVER_MODE) return ErrorCode::SOCKET_WRONG_OPERATION_MODE;
    struct sockaddr_in _targetAddress;
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
    NewTarget(_targetDescriptor, _targetAddress);
    return _targetDescriptor;
}

//***************************************************************************************
//--- Функция обработки отключения клиента ----------------------------------------------
//***************************************************************************************
void TCPDude::ClientDisconnected(SOCKET socketDescriptor) {
    for(ulong _i = 0; _i < targetsCount; _i++) {
        if(targetSockets[_i].Descriptor() == socketDescriptor) {
            if(_i < targetsCount - 1) {
                memcpy(&targetSockets[_i],
                       &targetSockets[_i+1],
                       sizeof(TargetSocket) * (targetsCount - _i));
            }
            if(ClientDisconnectedCallback) {
                ClientDisconnectedCallback(static_cast<int>(_i));
            }
            targetsCount --;
            memset(&targetSockets[targetsCount], 0, sizeof (TargetSocket));
            targetSockets = reinterpret_cast<TargetSocket*>(
                            realloc(targetSockets, sizeof (TargetSocket) *
                                    ((targetsCount == 0) ? 1 : targetsCount)));
            if(ClientDisconnectedCallback != nullptr) {
                ClientDisconnectedCallback(socketDescriptor);
            }
            break;
        }
    }
}

//***************************************************************************************
//--- Функция отключения клиента от сервера ---------------------------------------------
//***************************************************************************************
void TCPDude::DisconnectFromServer(SOCKET socketDescriptor) {
    if(operationMode == SERVER_MODE) return;
    targetSockets[0].Disconnect();
    this_thread::sleep_for(chrono::milliseconds(100));
#ifdef _WIN32
	shutdown(socketDescriptor, SD_BOTH);
	closesocket(socketDescriptor);
#elif __linux
	shutdown(socketDescriptor, SHUT_RDWR);
	close(socketDescriptor);
#endif

}

//***************************************************************************************
//--- Функция, возвращающая режим работы ------------------------------------------------
//***************************************************************************************
int TCPDude::GetOperationMode() {
    return operationMode;
}

//***************************************************************************************
//--- Функция возвращающая прерменную описания сокета по его адресу ---------------------
//***************************************************************************************
SOCKET TCPDude::GetSocketDescriptor(string address) {
    SOCKET _descriptor = -1;
	sockaddr_in _targetAddress;
    for (ulong _i = 0; _i < targetsCount; _i++) {
		inet_pton(AF_INET, address.data(), &_targetAddress.sin_addr);
        if(targetSockets[_i].Address().sin_addr.s_addr == _targetAddress.sin_addr.s_addr){
            _descriptor = targetSockets[_i].Descriptor();
            break;
        }
    }
    return _descriptor;
}

//***************************************************************************************
//--- Цикл ожидания подключения клиентов ------------------------------------------------
//***************************************************************************************
void TCPDude::ListenLoop(SOCKET socketDescriptor) {
    while (listenFlag) {
        sockaddr_in _clientAddress;
        int _len = sizeof (_clientAddress);
        SOCKET _clientDescriptor = accept(socketDescriptor,
                                       reinterpret_cast<sockaddr*>(&_clientAddress),
                                       reinterpret_cast<socklen_t*>(&_len));
        if(_clientDescriptor == -1 && listenFlag) {
            ErrorHandlerCallback(ErrorCode::CLIENT_ACCEPT_FAILED);
        } else {
            NewTarget(_clientDescriptor, _clientAddress);
        }
    }
}

//***************************************************************************************
//--- Функция обработки подключения новых сокетов ---------------------------------------
//***************************************************************************************
void TCPDude::NewTarget(SOCKET socketDescriptor, sockaddr_in clientAddress) {
    targetsCount ++;
    if(targetsCount > 1) {
        targetSockets = reinterpret_cast<TargetSocket*>(
                    realloc(targetSockets, sizeof (TargetSocket) * targetsCount));
    }
    targetSockets[targetsCount - 1] = TargetSocket(socketDescriptor, clientAddress);
    targetSockets[targetsCount - 1].socketThread = new thread(fReadLoop,
                reinterpret_cast<void*>(&targetSockets[targetsCount - 1]));

    if(ClientConnectedCallback) {
        ClientDisconnectedCallback(socketDescriptor);
    }
}

//***************************************************************************************
//--- Функция установки обратного вызова для данных -------------------------------------
//***************************************************************************************
void TCPDude::SetDataReadyCallback(function<void (string, uchar *, ulong)> Callback) {
    DataCallback = Callback;
}

//***************************************************************************************
//--- Функция установки обратного вызова для ошибок -------------------------------------
//***************************************************************************************
void TCPDude::SetErrorHandlerCallback(function<void (int)> Callback) {
    ErrorHandlerCallback = Callback;
}

//***************************************************************************************
//--- Функция установки обратного вызова сигнала о подключении клиента ------------------
//***************************************************************************************
void TCPDude::SetClientConnectedCallback(function<void (SOCKET)> Callback) {
    ClientConnectedCallback = Callback;
}

//***************************************************************************************
//--- Функция установки обратного вызова сигнала об отключении клиента ------------------
//***************************************************************************************
void TCPDude::SetClientDisconnectedCallback(function<void (SOCKET)> Callback) {
    ClientDisconnectedCallback = Callback;
}

//***************************************************************************************
//--- Функция запуска сервера -----------------------------------------------------------
//***************************************************************************************
void TCPDude::StartServer(uint16_t port) {
    if(operationMode == CLIENT_MODE) return;

    struct sockaddr_in _serverAddress;
    socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(socketDescriptor == -1) {
        if(ErrorHandlerCallback)
            ErrorHandlerCallback(ErrorCode::SOCKET_CREATION_FAILED);
        return;
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
        
        if(ErrorHandlerCallback)
            ErrorHandlerCallback(ErrorCode::SOCKET_BIND_FAILED);
        return;
    }
    if((listen(socketDescriptor, 3)) < 0) {

#ifdef _WIN32
		shutdown(socketDescriptor, SD_BOTH);
		closesocket(socketDescriptor);
#elif __linux
		shutdown(socketDescriptor, SHUT_RDWR);
		close(socketDescriptor);
#endif
        if(ErrorHandlerCallback)
            ErrorHandlerCallback(ErrorCode::SOCKET_LISTEN_FAILED);
        return;
    }
    listenFlag = true;
    listenThread = new thread(fListenLoop, socketDescriptor);
}

//***************************************************************************************
//--- Функция остановки сервера ---------------------------------------------------------
//***************************************************************************************
void TCPDude::StopServer() {
    if(operationMode == CLIENT_MODE) return;
    listenFlag = false;
    if (listenThread != nullptr) {
        listenThread->join();
    }
}

//***************************************************************************************
//--- Функция отправки данных -----------------------------------------------------------
//***************************************************************************************
void TCPDude::Send(SOCKET socketDescriptor, uchar *data, ulong size){

#ifdef _WIN32
	send(socketDescriptor, reinterpret_cast<char*>(data), size, 0);
#elif __linux
	send(socketDescriptor, data, size, 0);
#endif
}

//***************************************************************************************
//--- Деструктор ------------------------------------------------------------------------
//***************************************************************************************
TCPDude::~TCPDude() {

}
