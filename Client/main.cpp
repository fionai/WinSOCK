// CLIENT



#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
//это нужно, когда к проекту одновременно подключаются <WinSock2.h> и <Windows.h>
#endif // WIN32_LEAN_AND_MEAN


#include<iostream>
#include<Windows.h>
#include<WinSock2.h>
#include<WS2tcpip.h>
#include<iphlpapi.h>

using namespace std;

#pragma comment(lib, "WS2_32.lib"); //подгружает реализации функций из статической библиотеки для WS2tcpip.h

#define MTU 1500

void main()
{
	setlocale(LC_ALL, "");
	INT iResult = 0; //для отслеживания результатов выполнения функций
	
	//инициализация winsock:
	WSADATA wsaData;
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);  //MAKEWORD(2, 2) - выбираем версию WinSock
	if (iResult != 0)
	{
		cout << "WSATtartup failed with error: " << iResult << endl;
		return;
	}

	//Определяем параметры подключения
	addrinfo hints;
	addrinfo* target;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET; //family - семейство протоколов (стек протоколов) INET означает стек TCP/IPv4
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	iResult = getaddrinfo("127.0.0.1", "27015", &hints, &target);  //по символьному имени получает числовой адрес целевого узла
	if (iResult != 0)
	{
		cout << "getaddressinfo failed with error " << iResult << endl;
		WSACleanup();
		return;
	}

	//создаeм сокет для подключения к серверу
	SOCKET connect_socket = socket(target->ai_family, target->ai_socktype, target->ai_protocol);
	if (connect_socket == INVALID_SOCKET)
	{
		cout << "SOCKET creation failed with error: " << WSAGetLastError() << endl;
		freeaddrinfo(target);
		WSACleanup();
		return;
	}


	//Подключаемся к серверу
	iResult = connect(connect_socket, target->ai_addr, target->ai_addrlen);
	if (iResult != 0)
	{
		cout << "Connection failed with error: " << WSAGetLastError() << endl;
		closesocket(connect_socket);
		freeaddrinfo(target);
		WSACleanup();
		return;
	}
	freeaddrinfo(target);


	//отправка данных серверу
	CHAR send_buffer[MTU] = "Hello Server!!!";
	iResult = send(connect_socket,  send_buffer, strlen(send_buffer), NULL);
	if (iResult == SOCKET_ERROR)
	{
		cout << "Send failed with error: " << WSAGetLastError() << endl;
		closesocket(connect_socket);
		WSACleanup();
		return;
	}
	else cout << "Sent " << iResult << " bytes" << endl;

	//Получение данных от сервера:
	CHAR recv_buffer[MTU] = {};
	do
	{
		iResult = recv(connect_socket, recv_buffer, MTU, NULL);
		if (iResult > 0)  cout << recv_buffer << endl;
		else if (iResult == 0) cout << "Nothing received from Server" << endl;
		else cout << "Received failed with error: " << WSAGetLastError() << endl;
	} while (iResult > 0);

	//Разрываем tcp соединение
	iResult = shutdown(connect_socket, SD_BOTH);
	if (iResult != 0)
	{
		cout << "shutdown failed with error: " << WSAGetLastError << endl;
	}

	// Освобождаем ресурсы Winsock;
	closesocket(connect_socket);
	WSACleanup();
}