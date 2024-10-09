#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;
class TCPServer
{
	private:
		enum eErrorState{ERR_NONE, ERR_STARTUP, ERR_LISTENING, ERR_BIND, ERR_LISTEN, ERR_CLIENT_CONNECTION};

		bool verbose = true;	// Can be removed?
		bool echo = true;
		string ipAddress;
		int port;

		eErrorState errorState;
		int  errorCode;
		WORD winSockVerion = MAKEWORD(2, 2);	// set/use most recent version of windows sockets which is 2.2.  Not function call to startup requires it in a WORD data type
		WSADATA winSockData;					// struct which will hold required winsock data

		SOCKET listeningSocket = INVALID_SOCKET;
		SOCKET clientSocket = INVALID_SOCKET;
		sockaddr_in clientAddrInfo;				//store connected client information
		bool isClientConnected = false;

		void CreateSocket();
		void BindSocket();
		void AcceptClient();
		void Send(char buf[], int bytesSent);
		int GetNumberOfDigits(int number);
		string ParseCommands(string message);
		bool IsNumber(string n);
	public:
		TCPServer();
		TCPServer(string inIPAddress, int inPort);
		~TCPServer();
		bool Initialize();
		void Listen();
		void Receive();
		void ShutDown();
		void ErrorMessage();
		void ProcessIncomingMessage(string message, int bytesReceived);
};