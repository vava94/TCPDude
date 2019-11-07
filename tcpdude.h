#ifndef TCPSERVER_H
#define TCPSERVER_H
//***************************************************************************************
//--- Заголовочные ----------------------------------------------------------------------
//***************************************************************************************

#include <netdb.h>
#include <pthread.h>
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
        pthread_t socketThread;
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
        inline  pthread_t *Thread() { return &socketThread; }
        inline bool IsConnected() { return connected; }
    };

    ulong targetsCount = 0; //Количество клиентов сервера
    TargetSocket *targetSockets = nullptr;  // Массив клиентов, подключённых к серверу
    bool listenFlag = false;// Флаг, сервер слушает новых клиентов
    pthread_t listenThread; // Поток для цикла работы сервера
    int operationMode = -1; // Режим работы
    int socketDescriptor = 0;   // Дескриптор, описывающий сокет сервера

    static void *ReadLoop(void *arg);// Цикл прийма данных
    void ClientDisconnected(int socketDescriptor);  //Обработчик отключения клиента
    static void *ListenLoop(void*); // Цикл ожидания подключения клиентов
    // Функция обработки нового сокета
    void NewTarget(int clientDescriptor, sockaddr_in targetAddress);

public:
    // Перечисление режимов работы
    enum OperationMode {
        SERVER_MODE,
        CLIENT_MODE
    };
    // Конструктор
    TCPDude(int operationMode,
            void(*DataReadyCallback)(string address, uint8_t* data, size_t size),
            void(*ErrorCodeCallback)(int errorCode));
    ~TCPDude(); //Деструктор
    int GetOperationMode(); // Возвращает режим работы
    int GetSocketDescriptor(string address);
    void Send(int socketDescriptor, uint8_t *data, size_t size);
    //--- Сервер ------------------------------------------------------------------------
    void StartServer(uint16_t port); // Функция запуска сервера
    void StopServer();  // Функция остановки сервера
    //--- Клиент ------------------------------------------------------------------------
    // Функция подключения клиента к серверу
    int ClientConnectToServer(string address, unsigned short port);
};

#endif // TCP_SERVER_H
