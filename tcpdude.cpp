//***************************************************************************************
//--- Заголовочные ----------------------------------------------------------------------
//***************************************************************************************
//-- Локальные
#include "../shared_sources/errors.h"
#include "tcpdude.h"

//-- С++
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>


//***************************************************************************************
//--- Private static variables ----------------------------------------------------------
//***************************************************************************************

#define MAX_READ_BYTES 4096
using namespace std;
using namespace placeholders;

//***************************************************************************************
//--- Callbacks -------------------------------------------------------------------------
//***************************************************************************************


static void (*ErrorHandlerCallback)(int errorCode);

//***************************************************************************************
//--- Конструктор -----------------------------------------------------------------------
//***************************************************************************************
TCPDude::TCPDude(int operationMode,
                 void(*_DataReadyCallback)(string, uint8_t*, size_t),
                 void(*_ErrorHandlerCallback)(int)) {
    this->operationMode = operationMode;
    targetSockets = reinterpret_cast<TargetSocket*>(malloc(sizeof (TargetSocket)));
    ErrorHandlerCallback = _ErrorHandlerCallback;
    DataCallback = _DataReadyCallback;

    fNewTarget = bind(&TCPDude::NewTarget, this, _1, _2);
    fReadLoop = bind(&TCPDude::ReadLoop, this, _1, _2, _3);
    fListenLoop = bind(&TCPDude::ListenLoop, this, _1, _2, _3);
}

//***************************************************************************************
//--- Цикл приёма данных ----------------------------------------------------------------
//***************************************************************************************
void TCPDude::ReadLoop(void *targetSocket,
                        function<void(string, uint8_t*, size_t)> DataCallback,
                        function<void(int)> ClientDisconnected){
    TargetSocket *_targetSocket = reinterpret_cast<TargetSocket*>(targetSocket);
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
    uint8_t *_receiveBuffer = static_cast<uint8_t*>(malloc(MAX_READ_BYTES));
    long _bytesRead = 0;

    while (_targetSocket->IsConnected()) {
        bzero(_receiveBuffer, MAX_READ_BYTES);
        _bytesRead = read(_targetSocket->Descriptor(), _receiveBuffer, MAX_READ_BYTES);
        if(_bytesRead == 0) {
            _targetSocket->Disconnect();
        } else {
            DataCallback(_address, _receiveBuffer, static_cast<size_t>(_bytesRead));
        }
    }
    ClientDisconnected(_targetSocket->Descriptor());
}

//***************************************************************************************
//--- Функция подключения клиента к серверу ---------------------------------------------
//***************************************************************************************
int TCPDude::ClientConnectToServer(string address, unsigned short port) {
    if(operationMode == SERVER_MODE) return ErrorCode::SOCKET_WRONG_OPERATION_MODE;
    struct sockaddr_in _targetAddress;
    int _targetDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(_targetDescriptor == -1) {
        ErrorHandlerCallback(ErrorCode::SOCKET_CREATION_FAILED);
        return -1;
    }
    bzero(&_targetAddress, sizeof (_targetAddress));
    _targetAddress.sin_family = AF_INET;
    _targetAddress.sin_port = htons(port);

    if(inet_pton(AF_INET, address.data(), &_targetAddress.sin_addr) <= 0)
        return -1;

    if (connect(_targetDescriptor, reinterpret_cast<sockaddr *>(&_targetAddress),
                       sizeof(_targetAddress)) < 0) {
        ErrorHandlerCallback(ErrorCode::SOCKET_CONNECT_FAILED);
        return -1;
    }
    NewTarget(_targetDescriptor, _targetAddress);
    return _targetDescriptor;
}

//***************************************************************************************
//--- Функция обработки отключения клиента ----------------------------------------------
//***************************************************************************************
void TCPDude::ClientDisconnected(int socketDescriptor) {
    for(unsigned long _i = 0; _i < targetsCount; _i++) {
        if(targetSockets[_i].Descriptor() == socketDescriptor) {
            if(_i < targetsCount - 1) {
                memcpy(&targetSockets[_i],
                       &targetSockets[_i+1],
                       sizeof(TargetSocket) * (targetsCount - _i));
            }
            targetsCount --;
            bzero(&targetSockets[targetsCount], sizeof (TargetSocket));
            targetSockets = reinterpret_cast<TargetSocket*>(
                            realloc(targetSockets, sizeof (TargetSocket) *
                                    ((targetsCount == 0) ? 1 : targetsCount)));
            break;
        }
    }
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
int TCPDude::GetSocketDescriptor(string address) {
    int _descriptor = -1;
    for (ulong _i = 0; _i < targetsCount; _i++) {
        if(targetSockets[_i].Address().sin_addr.s_addr == inet_addr(address.data())){
            _descriptor = targetSockets[_i].Descriptor();
            break;
        }
    }
    return _descriptor;
}

//***************************************************************************************
//--- Цикл ожидания подключения клиентов ------------------------------------------------
//***************************************************************************************
void TCPDude::ListenLoop(int *socketDescriptor, bool *listenFlag, function<void(int, sockaddr_in)> NewTarget) {
    while (*listenFlag) {
        sockaddr_in _clientAddress;
        int _len = sizeof (_clientAddress);
        int _clientDescriptor = accept(*socketDescriptor,
                                       reinterpret_cast<sockaddr*>(&_clientAddress),
                                       reinterpret_cast<socklen_t*>(&_len));
        if(_clientDescriptor == -1 && *listenFlag) {
            ErrorHandlerCallback(ErrorCode::CLIENT_ACCEPT_FAILED);
        } else {
            NewTarget(_clientDescriptor, _clientAddress);
        }
    }
}

//***************************************************************************************
//--- Функция обработки подключения новых сокетов ---------------------------------------
//***************************************************************************************
void TCPDude::NewTarget(int socketDescriptor, sockaddr_in clientAddress) {
    printf("TCP descriptor: %d", socketDescriptor);
    targetsCount ++;
    if(targetsCount > 1) {
        targetSockets = reinterpret_cast<TargetSocket*>(
                    realloc(targetSockets, sizeof (TargetSocket) * targetsCount));
    }
    targetSockets[targetsCount - 1] = TargetSocket(socketDescriptor, clientAddress);
    targetSockets[targetsCount - 1].socketThread = new thread(fReadLoop,
                reinterpret_cast<void*>(&targetSockets[targetsCount - 1]), DataCallback);
}

//***************************************************************************************
//--- Функция запуска сервера -----------------------------------------------------------
//***************************************************************************************
void TCPDude::StartServer(uint16_t port) {
    if(operationMode == CLIENT_MODE) return;

    struct sockaddr_in _serverAddress;
    socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if(socketDescriptor == -1) {
        ErrorHandlerCallback(ErrorCode::SOCKET_CREATION_FAILED);
        return;
    }
    bzero(&_serverAddress, sizeof (_serverAddress));
    _serverAddress.sin_family = AF_INET;
    _serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    _serverAddress.sin_port = htons(port);
    if(bind(socketDescriptor, reinterpret_cast<sockaddr*>(&_serverAddress),
            sizeof (_serverAddress)) != 0) {
        close(socketDescriptor);
        ErrorHandlerCallback(ErrorCode::SOCKET_BIND_FAILED);
        return;
    }
    if((listen(socketDescriptor, 3)) < 0) {
        close(socketDescriptor);
        ErrorHandlerCallback(ErrorCode::SOCKET_LISTEN_FAILED);
        return;
    }
    listenFlag = true;
    listenThread = new thread(fListenLoop, &socketDescriptor, &listenFlag, fNewTarget);
}

//***************************************************************************************
//--- Функция остановки сервера ---------------------------------------------------------
//***************************************************************************************
void TCPDude::StopServer() {
    if(operationMode == CLIENT_MODE) return;

    listenFlag = false;
}

//***************************************************************************************
//--- Функция отправки данных -----------------------------------------------------------
//***************************************************************************************
void TCPDude::Send(int socketDescriptor, uint8_t *data, size_t size){
    write(socketDescriptor, data, size);
}

//***************************************************************************************
//--- Деструктор ------------------------------------------------------------------------
//***************************************************************************************
TCPDude::~TCPDude() {

}
