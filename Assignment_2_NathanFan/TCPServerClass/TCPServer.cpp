#include "TCPServer.h"
#define ENOUGH ((CHAR_BIT * sizeof(int) - 1) / 3 + 2)

// Nathan Fan
// 
// 

//--------------------------------------------------------------------------------
// Default Constructor
TCPServer::TCPServer()
{
	errorState = ERR_NONE;
	errorCode = 0;
	ipAddress = "127.0.0.1";
	port = 54000;
}

//--------------------------------------------------------------------------------
// Paramaterized constructor allows server to run on any ip address and port
TCPServer::TCPServer(string inIPAddress, int inPort)
{
	errorState = ERR_NONE;
	errorCode = 0;
	ipAddress = inIPAddress;
	port = inPort;
}
//--------------------------------------------------------------------------------
// Deconstructor cleans up socket resourses
TCPServer::~TCPServer()
{
	ShutDown();
}

//--------------------------------------------------------------------------------
// Initialized Winsock and calls the CreateSocket function if successful
bool TCPServer::Initialize()
{
	int result = WSAStartup(winSockVerion, &winSockData);		// startup winsock and ensure we are good to continue
	if (result == 0)										// all good, create and bind the socket
	{
		if (verbose)
		{
			cout << "Server status ..." << endl << ">>> WSA Start Up" << endl;
		}

		CreateSocket();

		if (errorState == ERR_NONE)						// if we have no errors creating and binding return true, we're ready to go
		{
			return true;
		}
	}
	else
	{
		errorState = ERR_STARTUP;							// set startup error code 
		errorCode = result;
		return false;
	}
}

//--------------------------------------------------------------------------------
// Creates the server listeing socket and calls BindSocket if successful
void TCPServer::CreateSocket()
{
	// Create the socket the server will listen to for applications wishing to connect to the server

	listeningSocket = socket(AF_INET, SOCK_STREAM, 0);  // create a socket which uses an IPv4 address, stream type socket with no set protocol (socket will figure it out)
	if (listeningSocket != INVALID_SOCKET)
	{
		if (verbose)
		{
			cout << ">>> Listening socket configured" << endl;
		}

		BindSocket();
	}
	else
	{
		errorState = ERR_LISTENING;
	}
}

//--------------------------------------------------------------------------------
// Binds the socket to the IP/port
void TCPServer::BindSocket()
{
	// Bind the socket to an IP address and port
	// In order to bind we need to fill in the socket address stucture prior to calling the bind function

	sockaddr_in severAddrInfo;								// socket address variable
	int serverAddrSize = sizeof(sockaddr_in);				// get the size of the structure as it is required for the bind call
	severAddrInfo.sin_family = AF_INET;						// set to IPv4
	severAddrInfo.sin_port = htons(port);					// choose port number - note, stay away from widely accepted port numbers
	inet_pton(AF_INET, ipAddress.c_str(), &severAddrInfo.sin_addr);	// convert the IP info to binary form and return via sin.addr member

	int result = bind(listeningSocket, (sockaddr*)& severAddrInfo, serverAddrSize);   // bind the listening socket with the specified IP info
	if (result != SOCKET_ERROR)								// check for successful binding of socket and address info
	{
		if (verbose)
		{
			cout << ">>> Socket binding complete" << endl;
		}
	}
	else
	{
		errorState == ERR_BIND;
	}
}

//--------------------------------------------------------------------------------
// Set the socket to listen for connection requests.  If we receive one call
// AcceptClient to accept the request
void TCPServer::Listen()
{
	// Set the socket to listen for connection requests 
	int result = listen(listeningSocket, SOMAXCONN);			// sets the socket to listen and sets the maximum size of pending request queue
	if (result != SOCKET_ERROR)
	{
		if (verbose)
		{
			cout << ">>> Listening socket active: IP: " << ipAddress << " Port: " << port << endl << ">>> Accepting connections" << endl;
		}
		AcceptClient();
	}
	else
	{
		errorState = ERR_LISTEN;
	}
}

//--------------------------------------------------------------------------------
// Create a client socket to accept communication for the client wishing to 
// establish a connection
void TCPServer::AcceptClient()
{
	// Create a client socket so you the server can receive the client that's attempting to connect.  At this point a client has made a connection request, now we have to accept them.
				
	int clientAddrSize = sizeof(sockaddr_in);	// get the size of the structure as it is required for the bind call

	clientSocket = accept(listeningSocket, (sockaddr*)& clientAddrInfo, &clientAddrSize);	// accept the client.  If we don't care about client connecting info set the last 2 parameters to NULL
	if (clientSocket != INVALID_SOCKET)
	{
		char host[NI_MAXHOST];		// client name array set to store the maximum allowable client remote name (1025 characters)
		char service[NI_MAXSERV];	// service (port) array set to store the maximum allowable service details (32 characters)

		ZeroMemory(host, NI_MAXHOST);		// macro to set the array memory to 0 - essentially initializing it to 0
		ZeroMemory(service, NI_MAXSERV);

		if (verbose)
		{
			cout << ">>> Client connection accepted" << endl << endl;
			if (getnameinfo((sockaddr*)& clientAddrInfo, clientAddrSize, host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0)		// gets the client info from the socket connection if we get it
			{
				cout << host << " connected on port " << service << endl;
			}
			else
			{
				inet_ntop(AF_INET, &clientAddrInfo.sin_addr, host, NI_MAXHOST);
				cout << host << " connected on port " << ntohs(clientAddrInfo.sin_port) << endl;
			}
		}

		isClientConnected = true;
		closesocket(listeningSocket);		// As we only want one client connection shutdown the listening port 
		listeningSocket = INVALID_SOCKET;
	}
	else			//  Client has connected successfully - lets get their dayta
	{
		errorState = ERR_CLIENT_CONNECTION;
	}
}

//--------------------------------------------------------------------------------
// Allow the server to receive client messages.  Send them back if server echo is
// enabled.
void TCPServer::Receive()
{
	// Sending a receiving data
	const int BUFFER_SIZE = 512;
	char buf[BUFFER_SIZE];

	if (verbose)
	{
		cout << ">>> Server actively receiving and sending messages..." << endl;
	}

	while (isClientConnected)
	{
		ZeroMemory(buf, BUFFER_SIZE);									// clear buffer prior to receiveing data each time 
		int bytesReceived = recv(clientSocket, buf, BUFFER_SIZE, 0);	// blocking line of code, will not pass recv until data is received

		if (bytesReceived > 0)									// process the results of the recv function
		{
			ProcessIncomingMessage(buf, bytesReceived);
		}
		else if (bytesReceived == 0)							// if the recv function returns with 0 bytes it's a client disconnection, exit the receive loop
		{
			cout << "Client disconnected.  Server shutting down..." << endl;
			isClientConnected = false;
		}
		else													// SOCKET - ERROR of some type
		{
			if (WSAGetLastError() == WSAECONNRESET)				// disconnect error - generally from a client app shutdown or other disconnection, exit the receive loop
			{
				cout << "Client disconnected. Shutting down" << endl;
				isClientConnected = false;
			}
			else												// other error, output the message
			{
				cout << "Problem reading from the client. Err#" << WSAGetLastError() << endl;
			}
		}
	}
}

//--------------------------------------------------------------------------------
//	send a message to the client
void TCPServer::Send(char buf[], int bytesSent)
{

	int sent = send(clientSocket, buf, bytesSent, 0);								// send the info we received back to the client
	if (sent != SOCKET_ERROR)
	{
		if (verbose)
		{
			if (strcmp(buf, "\r\n") == 0)
			{
				cout << "Sent: line feed " << endl;	// we received some bytes so output them to the console
			}
			else
			{
				cout << "Sent back to client: " << buf << endl;	// we received some bytes so output them to the console
			}
		}
	}
	else
	{
		cout << "Problem sending message to client. Err#" << WSAGetLastError() << endl;
	}
}

//--------------------------------------------------------------------------------
//	Cleanup Winsock resouces
void TCPServer::ShutDown()
{
	if (listeningSocket != INVALID_SOCKET)
	{
		closesocket(listeningSocket);
		listeningSocket = INVALID_SOCKET;
	}
	if (clientSocket != INVALID_SOCKET)
	{
		closesocket(clientSocket);
		clientSocket = INVALID_SOCKET;
	}
	WSACleanup();
}

//--------------------------------------------------------------------------------
// Error message helper function, prints out error based on obhects errorState.  
// errorState is set in various functions used to initialize Winsock
void TCPServer::ErrorMessage()
{
	if (errorState != ERR_NONE)
	{
		switch (errorState)
		{
			case ERR_STARTUP:
				cout << "Error starting up windows socket: " << errorCode << endl << endl;
				break;
			case ERR_LISTENING:
				cout << "Error creating socket: " << WSAGetLastError() << endl << endl;
				break;
			case ERR_BIND:
				cout << "Error binding socket: " << WSAGetLastError() << endl << endl;
				break;
			case ERR_LISTEN:
				cout << "Listen socket failure: " << WSAGetLastError() << endl << endl;
				break;
			case ERR_CLIENT_CONNECTION:
				cout << "Client connection failure: " << WSAGetLastError() << endl << endl;
				break;
		}
	}
}

//--------------------------------------------------------------------------------
// Processes all client messages and determines the appropriate action
void TCPServer::ProcessIncomingMessage(string message, int bytesReceived)
{
	string serverMessage;
	int sent;

	if (verbose)
	{
		if (message != "\r\n")			// don't display linefeed messages, all others are displayed
		{
			cout << "Received: " << bytesReceived << " byte[s] with message " << message << endl;	// we received some bytes so output them to the console
		}
	}
	// Commands: add, sub, mul, div

	// WHILE LOOP STARTS HERE

	// Can maybe check for spaces first since all elements require them
	// 
	size_t firstSpace = message.find(' ');
	size_t secondSpace = message.find(' ', firstSpace + 1);
	
	// does bottom line work?
	message += secondSpace;

	string firstNumber, secondNumber;

	// Empty command error checks
	if (message == "\r\n")
	{
		return;
	}
	// Missing space error
	else if ((firstSpace + 1) - (secondSpace - firstSpace - 1) == 0 || firstSpace == -1 || secondSpace == -1)
	{
		cout << endl << "inputted incomplete command, return empty" << endl;
		serverMessage = "Unknown command: " + message; 	// echo
	}
	// Command checks
	else
	{
		// Parse numbers
		firstNumber = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);
		secondNumber = message.substr(secondSpace + 1, message.length() - secondSpace - 1);
		
		cout << firstNumber << endl;
		cout << secondNumber << endl;

		// Space errors passed: one of the numbers are empty or not a number
		if (firstNumber == "" || secondNumber == "" || !IsNumber(firstNumber) || !IsNumber(secondNumber))
		{
			cout << endl << "inputted incomplete command, return empty" << endl;
			serverMessage = "Unknown command: " + message; 	// echo
		}
		// All good, run commands
		else
			serverMessage = ParseCommands(message, firstNumber, secondNumber);
	}

	// Message is guarenteed, safety error check
	if (serverMessage.length() > 0)		// if we have a server message send it back to the client
	{
		serverMessage += "\r\n";
		Send(&serverMessage[0], serverMessage.length());
	}
}

int TCPServer::GetNumberOfDigits(int number)
{
	int r = 1;
	if (number < 0) number = (number == INT_MIN) ? INT_MAX : -number;
	while (number > 9) {
		number /= 10;
		r++;
	}
	return r;
}

string TCPServer::ParseCommands(string message, string firstNumber, string secondNumber)
{
	int variableA = stoi(firstNumber);
	int variableB = stoi(secondNumber);
	char finalOutput[ENOUGH];

	// Check the client message to see if it matches a "command" and take appropriate action
	if (message.find("add") == 0)				// Add command
	{
		int total = variableA + variableB;
		sprintf_s(finalOutput, "%d", total);

		cout << "Use has asked us to add " << variableA << " with " << variableB << " which equals: " << finalOutput << endl;

		return firstNumber + " + " + secondNumber + " = " + finalOutput;
	}
	else if (message.find("sub") == 0)			// Subtract command
	{
		int total = variableA - variableB;
		sprintf_s(finalOutput, "%d", total);

		cout << "Use has asked us to subtract " << variableA << " with " << variableB << " which equals: " << finalOutput << endl;

		return firstNumber + " - " + secondNumber + " = " + finalOutput;
	}
	else if (message.find("mul") == 0)			// Multiply command
	{
		int total = variableA * variableB;
		sprintf_s(finalOutput, "%d", total);

		cout << "Use has asked us to multiply " << variableA << " with " << variableB << " which equals: " << finalOutput << endl;

		return firstNumber + " x " + secondNumber + " = " + finalOutput;
	}
	else if (message.find("div") == 0)			// Divide command
	{
		// Cannot divide by 0 error check
		if (variableB == 0)
		{
			cout << "Attempted to divide by 0 : ERROR" << endl;
			return  "Attempted to divide by 0 : ERROR";
		}

		// Must be float to see decimals
		float variableA = stof(firstNumber);
		float variableB = stof(secondNumber);

		float total = variableA / variableB;
		sprintf_s(finalOutput, "%f", total);

		cout << "Use has asked us to divide " << variableA << " with " << variableB << " which equals: " << finalOutput << endl;

		return firstNumber + " / " + secondNumber + " = " + finalOutput;
	}
	else										// Error pass check
	{
		if (echo && (message != "\r\n"))
			return "Unknown command: " + message; 	// echo
	}
}

bool TCPServer::IsNumber(string input)
{
	int i = input.length();
	bool isnum = (i > 0);

	while (i-- && isnum) {
		if (i == 0 && input[i] == '-')
			continue;
		if (!(input[i] >= '0' && input[i] <= '9'))
			return false;
	}
	return true;
}
