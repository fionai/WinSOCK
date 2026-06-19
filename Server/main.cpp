// SERVER
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif 

#include <iostream>
#include <Windows.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string> 

using namespace std;

#pragma comment(lib, "WS2_32.lib")

#define MTU 1500
#define MAX_CONNECTIONS 3

SOCKET clients[MAX_CONNECTIONS] = {};
DWORD dwThreadIDs[MAX_CONNECTIONS] = {};
HANDLE hThreads[MAX_CONNECTIONS] = {};
INT g_ActiveClients = 0;

CRITICAL_SECTION cs;

INT GetClientIndex(DWORD dwThreadID);
VOID Shift(INT index);
VOID Broadcast(const char* sz_message, SOCKET sender_socket);
DWORD WINAPI ClientHandle(LPVOID lpParam);

void main()
{
	setlocale(LC_ALL, "");
	cout << "SERVER" << endl;

	InitializeCriticalSection(&cs);

	INT iResult = 0;
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	addrinfo hints;
	addrinfo* binder = nullptr;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	iResult = getaddrinfo(NULL, "27015", &hints, &binder);
	if (iResult != 0)
	{
		cout << "getaddressinfo() failed with error: " << iResult << endl;
		WSACleanup();
		DeleteCriticalSection(&cs);
		return;
	}

	SOCKET listen_socket = socket(binder->ai_family, binder->ai_socktype, binder->ai_protocol);
	if (listen_socket == INVALID_SOCKET)
	{
		cout << "SOCKET creation failed with error: " << WSAGetLastError() << endl;
		freeaddrinfo(binder);
		WSACleanup();
		DeleteCriticalSection(&cs);
		return;
	}

	iResult = bind(listen_socket, binder->ai_addr, binder->ai_addrlen);
	freeaddrinfo(binder);
	if (iResult == SOCKET_ERROR)
	{
		cout << "Bind failed with error " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		DeleteCriticalSection(&cs);
		return;
	}

	if (listen(listen_socket, MAX_CONNECTIONS) == SOCKET_ERROR)
	{
		cout << "listen failed with error " << WSAGetLastError() << endl;
		closesocket(listen_socket);
		WSACleanup();
		DeleteCriticalSection(&cs);
		return;
	}

	do
	{
		SOCKADDR_IN client_addr;
		int client_addrlen = sizeof(client_addr);
		SOCKET client_socket = accept(listen_socket, (SOCKADDR*)&client_addr, &client_addrlen);
		if (client_socket == INVALID_SOCKET)
		{
			cout << "Accept failed with error " << WSAGetLastError() << endl;
			continue;
		}

		cout << "CONNECTED ON " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << endl;

		EnterCriticalSection(&cs);
		if (g_ActiveClients < MAX_CONNECTIONS)
		{
			clients[g_ActiveClients] = client_socket;

			hThreads[g_ActiveClients] = CreateThread
			(
				NULL,
				0,
				ClientHandle,
				(LPVOID)client_socket, // Передаем только сокет, как в вашем исходном коде
				0,
				&dwThreadIDs[g_ActiveClients]
			);
			g_ActiveClients++;
			LeaveCriticalSection(&cs);
		}
		else
		{
			LeaveCriticalSection(&cs);
			CHAR szDeclineMessage[] = "Подключение невозможно, все места заняты.";
			send(client_socket, szDeclineMessage, (int)strlen(szDeclineMessage), 0);
			shutdown(client_socket, SD_BOTH);
			closesocket(client_socket);
		}
	} while (true);

	closesocket(listen_socket);
	WSACleanup();
	DeleteCriticalSection(&cs);
}

INT GetClientIndex(DWORD dwThreadID)
{
	for (INT i = 0; i < g_ActiveClients; i++)
	{
		if (dwThreadIDs[i] == dwThreadID) return i;
	}
	return -1;
}

VOID Shift(INT index)
{
	if (index == -1) return;

	if (hThreads[index] != NULL) {
		CloseHandle(hThreads[index]);
	}

	for (INT i = index; i < g_ActiveClients - 1; i++)
	{
		clients[i] = clients[i + 1];
		dwThreadIDs[i] = dwThreadIDs[i + 1];
		hThreads[i] = hThreads[i + 1];
	}
	clients[MAX_CONNECTIONS - 1] = NULL;
	dwThreadIDs[MAX_CONNECTIONS - 1] = 0;
	hThreads[MAX_CONNECTIONS - 1] = NULL;
	g_ActiveClients--;
}

VOID Broadcast(const char* sz_message, SOCKET sender_socket)
{
	EnterCriticalSection(&cs);
	for (INT i = 0; i < g_ActiveClients; i++)
	{
		// Отправляем сообщение всем, КРОМЕ отправителя
		if (clients[i] != sender_socket)
		{
			send(clients[i], sz_message, (int)strlen(sz_message), 0);
		}
	}
	LeaveCriticalSection(&cs);
}

DWORD WINAPI ClientHandle(LPVOID lpParam)
{
	SOCKET client_socket = (SOCKET)lpParam;

	// Безопасно узнаем IP и порт клиента прямо через его сокет
	SOCKADDR_IN client_addr;
	int addr_len = sizeof(client_addr);
	getpeername(client_socket, (SOCKADDR*)&client_addr, &addr_len);

	string client_ip = inet_ntoa(client_addr.sin_addr);
	int client_port = ntohs(client_addr.sin_port);

	INT iResult = 0;
	CHAR recv_buffer[MTU] = {};

	do
	{
		ZeroMemory(recv_buffer, MTU);
		iResult = recv(client_socket, recv_buffer, MTU, 0);
		if (iResult > 0)
		{
			cout << "From " << client_ip << ":" << client_port << " received: " << recv_buffer << endl;

			// Формируем сообщение с IP и портом
			string formatted_message = "[" + client_ip + ":" + to_string(client_port) + "]: " + recv_buffer;

			Broadcast(formatted_message.c_str(), client_socket);
		}
		else if (iResult == 0)
		{
			cout << "Client " << client_ip << ":" << client_port << " disconnected" << endl;
		}
		else
		{
			// Если сокет закрывается нормально при выходе, это не ошибка
			DWORD err = WSAGetLastError();
			if (err != WSAECONNRESET) {
				cout << "Receive failed with error " << err << endl;
			}
		}
	} while (iResult > 0);

	shutdown(client_socket, SD_BOTH);
	closesocket(client_socket);

	EnterCriticalSection(&cs);
	Shift(GetClientIndex(GetCurrentThreadId()));
	LeaveCriticalSection(&cs);

	return 0;
}