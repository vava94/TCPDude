#pragma once
//***************************************************************************************
//--- Заголовочные ----------------------------------------------------------------------
//***************************************************************************************
#include <functional>

#include <thread>
#include <vector>
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

//***************************************************************************************
//--- Класс, осуществляющий приём и передачу данных по протоколу TCP. Может работать  ---
//--- как в режиме есервера, так и в режиме клиента. ------------------------------------
//***************************************************************************************
class TCPDude {

private:
    // Структура удалённого сокета
    struct TargetSocket {
    private:
        bool connected = true;
        SOCKET socketDescriptor;

        sockaddr_in socketAddress;

    public:
        TargetSocket(SOCKET socketDescriptor, sockaddr_in socketAddress) {
            this->socketDescriptor = socketDescriptor;
            this->socketAddress = socketAddress;
        }

        inline void Disconnect() { connected = false; }
        inline sockaddr_in Address() { return socketAddress; }
        inline sockaddr_in* AddressPtr() { return &socketAddress; }
        inline SOCKET Descriptor() { return socketDescriptor; }
        std::thread *socketThread = nullptr;
        inline bool IsConnected() { return connected; }
    };

    bool listenFlag = false;// Флаг, сервер слушает новых клиентов
    int operationMode = -1; // Режим работы
    SOCKET socketDescriptor = 0;   // Дескриптор, описывающий сокет сервера
    TargetSocket *targetSockets = nullptr;  // Массив клиентов, подключённых к серверу
    std::thread *listenThread; // Поток для цикла работы сервера
    ulong targetsCount = 0; //Количество клиентов сервера

    std::function<void(std::string, uchar*, ulong)> DataCallback = nullptr;
	std::function<void(SOCKET)> ClientConnectedCallback = nullptr;
	std::function<void(SOCKET)> ClientDisconnectedCallback = nullptr;
	std::function<void(int)> ErrorHandlerCallback = nullptr;
	std::function<void(void*)> fReadLoop;
	std::function<void(SOCKET)> fListenLoop;

    void ReadLoop(void *targetSocket); // Цикл приёма данных

    void ClientDisconnected(SOCKET socketDescriptor);  //Обработчик отключения клиента
    void ListenLoop(SOCKET); // Цикл ожидания подключения клиентов
    // Функция обработки нового сокета
    void NewTarget(SOCKET clientDescriptor, sockaddr_in targetAddress);

public:
    // Перечисление режимов работы
    enum OperationMode {
        SERVER_MODE,
        CLIENT_MODE
    };
    // Конструктор
    TCPDude(int operationMode);
    ~TCPDude(); //Деструктор
    int GetOperationMode(); // Возвращает режим работы
    SOCKET GetSocketDescriptor(std::string address);
    void DataReadyCallback(std::string, uchar*, ulong);
    void SetDataReadyCallback(std::function<void(std::string, uchar*, ulong)> DataCallback);
    void SetClientConnectedCallback(std::function<void(SOCKET socketDescriptor)> ConnectedCallback);
    void SetClientDisconnectedCallback(std::function<void(SOCKET socketDescriptor)> ConnectedCallback);
    void SetErrorHandlerCallback(std::function<void(int)> ErrorHandlerCallback);
    void Send(SOCKET socketDescriptor, uchar *data, ulong size);
    //--- Сервер ------------------------------------------------------------------------
    void StartServer(uint16_t port); // Функция запуска сервера
    void StopServer();  // Функция остановки сервера
    //--- Клиент ------------------------------------------------------------------------
    // Функция подключения клиента к серверу
    SOCKET ClientConnectToServer(std::string address, unsigned short port);
    void DisconnectFromServer(SOCKET socketDescriptor);
};