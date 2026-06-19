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

DWORD WINAPI Receive(LPVOID lpParam);

void main()
{
	setlocale(LC_ALL, "");
	INT iResult = 0; //для отслеживания результатов выполнения функций
	SetConsoleCP(1251);
	SetConsoleOutputCP(1251);
	
	cout << "=== CLIENT ===" << endl;

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

	cout << "Успешно подключено к серверу!" << endl;
	cout << "Для выхода введите 'exit'" << endl;


	// Передаем сокет в поток чтения
	DWORD dwThreadID = 0;
	HANDLE hReceiveThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Receive, (LPVOID)connect_socket, NULL, &dwThreadID);

	//5) отправка данных серверу
	CHAR send_buffer[MTU] = "Hello Server!!!";

	// Главный цикл отправки сообщений
	do
	{
		cout << "Вы: ";
		cin.getline(send_buffer, MTU);

		// Если ввели exit — выходим и закрываем приложение
		if (_stricmp(send_buffer, "exit") == 0) {
			break;
		}

		if (strlen(send_buffer) == 0)
		{
			continue;
		}

		if (strlen(send_buffer) > 0) {
			iResult = send(connect_socket, send_buffer, (int)strlen(send_buffer), 0);
			if (iResult == SOCKET_ERROR)
			{
				cout << "\n[Ошибка] Отправка не удалась. Код ошибки: " << WSAGetLastError() << endl;
				break;
			}
		}
	} while (true);

	cout << "Разрыв соединения..." << endl;
	shutdown(connect_socket, SD_BOTH);
	closesocket(connect_socket);

	if (hReceiveThread != NULL) {
		WaitForSingleObject(hReceiveThread, 1000);
		CloseHandle(hReceiveThread);
	}

	WSACleanup();
}


DWORD WINAPI Receive(LPVOID lpParam)
{
	SOCKET connect_socket = (SOCKET)lpParam; // Восстанавливаем сокет из указателя
	INT iResult = 0;
	CHAR recv_buffer[MTU] = {};

	do
	{
		ZeroMemory(recv_buffer, sizeof(recv_buffer));
		iResult = recv(connect_socket, recv_buffer, MTU, 0);

		if (iResult > 0)
		{
			//  вывод строки, содержащей IP и порт от сервера, с переносом "Вы: "
			cout << "\r" << recv_buffer << "\nВы: ";
		}
		else if (iResult == 0)
		{
			cout << "\n[Сервер]: Соединение закрыто сервером." << endl;
			break; //  выход из цикла, чтобы избежать 100% нагрузки на CPU
		}
		else
		{
			DWORD error = WSAGetLastError();
			// Если сокет закрыт нами же из main, ошибку выводить не нужно
			if (error != WSAESHUTDOWN && error != WSAECONNRESET) {
				cout << "\n[Ошибка приема]: " << error << endl;
			}
			break; //  выход из цикла при ошибке сети
		}
	} while (true);
	
	return 0;
}