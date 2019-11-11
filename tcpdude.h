#ifndef TCPSERVER_H
#define TCPSERVER_H
//***************************************************************************************
//--- Заголовочные ----------------------------------------------------------------------
//***************************************************************************************
#include <functional>
#include <netdb.h>
#include <thread>
#include <vector>
#include <string>

using namespace std;
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
        int socketDescriptor;

        sockaddr_in socketAddress;

    public:
        TargetSocket(int socketDescriptor, sockaddr_in socketAddress) {
            this->socketDescriptor = socketDescriptor;
            this->socketAddress = socketAddress;
        }

        inline void Disconnect() { connected = false; }
        inline sockaddr_in Address() { return socketAddress; }
        inline sockaddr_in* AddressPtr() { return &socketAddress; }
        inline int Descriptor() { return socketDescriptor; }
        std::thread *socketThread = nullptr;
        inline bool IsConnected() { return connected; }
    };

    bool listenFlag = false;// Флаг, сервер слушает новых клиентов
    int operationMode = -1; // Режим работы
    int socketDescriptor = 0;   // Дескриптор, описывающий сокет сервера
    TargetSocket *targetSockets = nullptr;  // Массив клиентов, подключённых к серверу
    thread *listenThread; // Поток для цикла работы сервера
    ulong targetsCount = 0; //Количество клиентов сервера

    function<void(string, uint8_t*, size_t)> DataCallback = nullptr;
    function<void(int)> ClientConnectedCallback = nullptr;
    function<void(int)> ClientDisconnectedCallback = nullptr;
    function<void(int)> ErrorHandlerCallback = nullptr;
    function<void(void*)> fReadLoop;
    function<void(int)> fListenLoop;

    void ReadLoop(void *targetSocket); // Цикл приёма данных

    void ClientDisconnected(int socketDescriptor);  //Обработчик отключения клиента
    void ListenLoop(int); // Цикл ожидания подключения клиентов
    // Функция обработки нового сокета
    void NewTarget(int clientDescriptor, sockaddr_in targetAddress);

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
    int GetSocketDescriptor(string address);
    void DataReadyCallback(string, uint8_t*, size_t);
    void SetDataReadyCallback(function<void(string, uint8_t*, size_t)> DataCallback);
    void SetClientConnectedCallback(function<void(int socketDescriptor)> ConnectedCallback);
    void SetClientDisconnectedCallback(function<void(int socketDescriptor)> ConnectedCallback);
    void SetErrorHandlerCallback(function<void(int)> ErrorHandlerCallback);
    void Send(int socketDescriptor, uint8_t *data, size_t size);
    //--- Сервер ------------------------------------------------------------------------
    void StartServer(uint16_t port); // Функция запуска сервера
    void StopServer();  // Функция остановки сервера
    //--- Клиент ------------------------------------------------------------------------
    // Функция подключения клиента к серверу
    int ClientConnectToServer(string address, unsigned short port);
    void DisconnectFromServer(int socketDescriptor);
};

#endif // TCP_SERVER_H
