// SERVER


#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif // !WIN32_LEAN_AND_MEAN


#include<iostream>
#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>

using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define MTU 1500

void main()
{
	setlocale(LC_ALL, "");
	cout << "SERVER" << endl;
	INT iResult = 0;
	// 1) init WinSOCK
	WSADATA wsaData;
	iResult =  WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0) 
	{
		cout << "WSAStartup failed with error: " << iResult << endl;
		return;
	}


	//2) Параметры подключения
	addrinfo hints;
	addrinfo* binder = nullptr;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, "27015", &hints, &binder);//NULL означает 0.0.0.0, т.е. сокет будет прослушивать все ип адреса
	if (iResult != 0)
	{
		cout << "getaddressinfo() failed with error: " << iResult << endl;
		WSACleanup();
		return;
	}

	//3) Сщздаем сокет, который будет прослушивать канал и принимать сообщения от клиентов
	SOCKET listen_socket = socket(binder->ai_family, binder->ai_socktype, binder->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "SOCKET creation failed with error: " << WSAGetLastError() << endl;
		freeaddrinfo(binder);
		WSACleanup();
		return;
	}

	//4)bind socket - привызываем сокет к ip адресам и портам, которые он будует слушать
	iResult = bind(listen_socket, binder->ai_addr, binder->ai_addrlen);
	freeaddrinfo(binder);
	if (iResult == SOCKET_ERROR)
	{
		cout << "Bind failed with error " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//5) запускаем прослушивание 
	if (listen(listen_socket, 1) == SOCKET_ERROR) // 1 - максимальное количество подключений
	{
		cout << "listen failed with error " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//6) обработка входящих соединений
	SOCKET client_socket = accept(listen_socket, NULL, NULL); // ожидает запрос от клиента
	if (client_socket == INVALID_SOCKET)
	{
		cout << "Accept failed with error " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		return;
	}

	//7) получение и отправка данных
	CHAR send_buffer[MTU] = "Hello Client!!!";
	CHAR recv_buffer[MTU] = {};
	do
	{
		ZeroMemory(recv_buffer, MTU);
		iResult = recv(client_socket, recv_buffer, MTU-1, 0);

					
		if (iResult > 0)
		{
			cout << iResult << " bytes received, Message: " << recv_buffer << endl;

			if (_stricmp(recv_buffer, "exit") == 0)
			{
				cout << "Client send \"exit\". Closing connection." << endl;
				break;
			}

			INT iSendResult = send(client_socket, send_buffer, strlen(send_buffer), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				cout << "Send failed with error " << WSAGetLastError() << endl;
				closesocket(client_socket);
				break;
			}
			else cout << iSendResult << " bytes send" << endl;
		}
		else if (iResult == 0) cout << "nothing received from client" << endl;
		else cout << "Receive failed with error " << WSAGetLastError() << endl;
	} while (iResult > 0);

	//8) разрываем соединение с клиентом
	iResult = shutdown(client_socket, SD_BOTH);
	if (iResult == SOCKET_ERROR) cout << "shutdown failed with error " << WSAGetLastError() << endl;
	closesocket(client_socket);

	//9)Release resources
	closesocket(listen_socket);
	WSACleanup();
	cout << "Server closed" << endl;
}