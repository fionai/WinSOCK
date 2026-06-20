// SERVER

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

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
#define MAX_CONNECTIONS  3

SOCKET clients[MAX_CONNECTIONS] = {};
DWORD dwThreadIDs[MAX_CONNECTIONS] = {};
HANDLE hThreads[MAX_CONNECTIONS] = {};
INT g_ActiveClients = 0;

INT GetClientIndex(DWORD dwThreadID);
VOID Shift(INT index);
VOID Broadcast(CHAR sz_message[], DWORD dwID);
VOID ClientHandle(SOCKET client_socket);

void main()
{
	setlocale(LC_ALL, "");
	cout << "SERVER" << endl;
	INT iResult = 0;
	// 1) init WinSOCK
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	//2) Параметры подключения
	addrinfo hints;
	addrinfo* binder;
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
	do
	{
		SOCKADDR_IN client_addr;
		int client_addrlen = sizeof(client_addr);
		SOCKET client_socket = accept(listen_socket, (SOCKADDR*)&client_addr, &client_addrlen); // ожидает запрос от клиента
		if (client_socket == INVALID_SOCKET)
		{
			cout << "Accept failed with error " << WSAGetLastError() << endl;

		}
		cout << "CONNECTED ON " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;

		//7) получение и отправка данных
		if (g_ActiveClients < MAX_CONNECTIONS)
		{
			//ClientHandle(client_socket);
			clients[g_ActiveClients] = client_socket;
			hThreads[g_ActiveClients] = CreateThread
			(
				NULL, //атрибуты безопасности
				NULL, //размер стека. Если 0, то все потоки будут использовать стек своего родительского процесса
				(LPTHREAD_START_ROUTINE)ClientHandle, //указатель на функцию, которая будет выполняться в потоке
				(LPVOID)client_socket, //параметр, передаваемый в функцию. Функция, запускаемаяв потоке, может принимать не более 1 параметра. 
				//Если функиця, запускаемая в потоке, не принимает параметров, сюда передается NULL
				NULL, //флаги создания потока
				&dwThreadIDs[g_ActiveClients]
			);
			g_ActiveClients++;
		}
		else
		{
			CHAR szDeclineMessage[] = "Подключение невозможно, все места заняты.";

			iResult = send(client_socket, szDeclineMessage, strlen(szDeclineMessage), 0);
			if (iResult == SOCKET_ERROR) cout << "send error " << WSAGetLastError() << endl;
			iResult = shutdown(client_socket, SD_BOTH);
			if (iResult == SOCKET_ERROR) cout << "shutdown error " << WSAGetLastError() << endl;
		}
	//8) разрываем соединение с клиентом
	} while (true);



	//9)Release resources
	closesocket(listen_socket);
	WSACleanup();
}


INT GetClientIndex(DWORD dwThreadID)
{
	for (INT i = 0; i < g_ActiveClients; i++)
	{
		if (dwThreadIDs[i] == dwThreadID) return i;
	}
}
VOID Shift(INT index)
{
	for (INT i = index; i < g_ActiveClients; i++)
	{
		clients[i] = clients[i + 1];
		dwThreadIDs[i] = dwThreadIDs[i + 1];
		hThreads[i] = hThreads[i + 1];
	}
	clients[MAX_CONNECTIONS - 1] = NULL;
	dwThreadIDs[MAX_CONNECTIONS - 1] = NULL;
	hThreads[MAX_CONNECTIONS - 1] = NULL;
	g_ActiveClients--;
}
VOID Broadcast(CHAR sz_message[], DWORD dwID)
{
	for (INT i = 0; i < g_ActiveClients; i++)
	{
		if (dwID != dwThreadIDs[i])
		send(clients[i], sz_message, strlen(sz_message), 0);
	}
}
VOID ClientHandle(SOCKET client_socket)
{
	SOCKADDR_IN client_address;
	client_address.sin_family = AF_INET;
	INT addrlen = sizeof(client_address);
	getpeername(client_socket, (SOCKADDR*)&client_address, &addrlen);
	CHAR sz_client_address[32] = {};
	//CHAR sz_client_connected[32] = {};
	sprintf(sz_client_address, "[%s:%d]:\t", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));


	INT iResult = 0;
	//7) получение и отправка данных
	CHAR send_buffer[MTU] = "Hello Client!!!";
	CHAR recv_buffer[MTU] = {};
	do
	{
		ZeroMemory(recv_buffer, MTU);
		ZeroMemory(send_buffer, MTU);
		iResult = recv(client_socket, recv_buffer, MTU, 0);
		if (iResult > 0)
		{
			sprintf(send_buffer, "%s%s", sz_client_address, recv_buffer);
			cout << iResult << " bytes received, Message: " << recv_buffer << endl;
			Broadcast(send_buffer, GetCurrentThreadId());
	/*		INT iSendResult = send(client_socket, send_buffer, strlen(send_buffer), 0);
			if (iSendResult == SOCKET_ERROR)
			{
				cout << "Send failed with error " << WSAGetLastError() << endl;
				closesocket(client_socket);
			}
			else cout << iSendResult << " bytes send" << endl;*/
		}
		else if (iResult == 0) cout << "nothing received from client" << endl;
		else cout << "Receive fsiled with error " << WSAGetLastError() << endl;
	} while (iResult > 0);

	//8) разрываем соединение с клиентом
	iResult = shutdown(client_socket, SD_BOTH);
	if (iResult == SOCKET_ERROR) cout << "shutdown failed with error " << WSAGetLastError() << endl;
	closesocket(client_socket);
	Shift(GetClientIndex(GetCurrentThreadId()));
	ExitThread(0);
}