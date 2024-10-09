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

		// Intro message:
		string stringMessage = "      Wecome to ACS (Awesome Calculator Server)\r\n----------------------------------------------------\r\nSupported commands: \"add\", \"sub\", \"mul\", \"div\"\r\nUsage: command # # # ...\r\nUsage sample: add 2 2 yields \"2 + 2 = 4\"\r\nGo a head, give it a whirl!\r\n\r\n";

		Send(&stringMessage[0], stringMessage.length());
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

	// Empty command error checks
	if (message == "\r\n") return;
	
	// Command checks
	// Commands: add, sub, mul, div
	serverMessage = "\r\nProcessing calculation ...\r\n" + ParseCommands(message) + "\r\n";

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

string TCPServer::ParseCommands(string message)
{
	string returnBuffer;
	returnBuffer.clear();

	int buffer = 0;
	float total = 0;

	// Final total char setup
	char finalOutput[ENOUGH];

	// Check the client message to see if it matches a "command" and take appropriate action
	if (message.find("add") == 0)				// Add command
	{
		while (true)
		{
			string firstNumber;
			
			size_t firstSpace = message.find(' ', buffer);
			size_t secondSpace = message.find(' ', firstSpace + 1);

			// Missing space error
			if (secondSpace == -1)
			{
				secondSpace = message.length();
				cout << "Second space error - reset" << endl;
			}
			else if (firstSpace == -1)
				break;
			else if ((firstSpace + 1) - (secondSpace - firstSpace - 1) == 0)
			{
				cout << endl << "inputted incomplete command of spaces, return empty" << endl;
				return "Unknown command: " + message; 	// echo
			}

			// Parse numbers
			firstNumber = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);

			cout << "Read number: " + firstNumber << endl;

			// Space errors passed: one of the numbers are empty or not a number
			if (firstNumber == "" || !IsNumber(firstNumber))
			{
				cout << endl << "inputted numbers are not valid, return empty" << endl;
				break;
			}

			// string format number to int format
			float variableA = stof(firstNumber);
			
			// Calculate total
			if (buffer == 0)
				total = variableA;
			else
				total += variableA;

			buffer = secondSpace;
			returnBuffer += returnBuffer.empty() ? firstNumber : " + " + firstNumber;
		}
		// Convert to char array
		sprintf_s(finalOutput, "%f", total);
		
		return returnBuffer + " = " + finalOutput;
	}
	else if (message.find("sub") == 0)			// Subtract command
	{
		while (true)
		{
			string firstNumber;
			
			size_t firstSpace = message.find(' ', buffer);
			size_t secondSpace = message.find(' ', firstSpace + 1);

			// Missing space error
			if (secondSpace == -1)
			{
				secondSpace = message.length();
				cout << "Second space error - reset" << endl;
			}
			else if (firstSpace == -1)
				break;
			else if ((firstSpace + 1) - (secondSpace - firstSpace - 1) == 0)
			{
				cout << endl << "inputted incomplete command of spaces, return empty" << endl;
				return "Unknown command: " + message; 	// echo
			}

			// Parse numbers
			firstNumber = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);

			cout << "Read number: " + firstNumber << endl;

			// Space errors passed: one of the numbers are empty or not a number
			if (firstNumber == "" || !IsNumber(firstNumber))
			{
				cout << endl << "inputted numbers are not valid, return empty" << endl;
				break;
			}

			// string format number to int format
			float variableA = stof(firstNumber);

			// Calculate total
			if (buffer == 0)
				total = variableA;
			else
				total -= variableA;

			buffer = secondSpace;
			returnBuffer += returnBuffer.empty() ? firstNumber : " - " + firstNumber;
		}
		// Convert to char array
		sprintf_s(finalOutput, "%f", total);

		return returnBuffer + " = " + finalOutput;
	}
	else if (message.find("mul") == 0)			// Multiply command
	{
		while (true)
		{
			string firstNumber;
			
			size_t firstSpace = message.find(' ', buffer);
			size_t secondSpace = message.find(' ', firstSpace + 1);

			// Missing space error
			if (secondSpace == -1)
			{
				secondSpace = message.length();
				cout << "Second space error - reset" << endl;
			}
			else if (firstSpace == -1)
				break;
			else if ((firstSpace + 1) - (secondSpace - firstSpace - 1) == 0)
			{
				cout << endl << "inputted incomplete command of spaces, return empty" << endl;
				return "Unknown command: " + message; 	// echo
			}

			// Parse numbers
			firstNumber = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);

			cout << "Read number: " + firstNumber << endl;

			// Space errors passed: one of the numbers are empty or not a number
			if (firstNumber == "" || !IsNumber(firstNumber))
			{
				cout << endl << "inputted numbers are not valid, return empty" << endl;
				break;
			}

			// string format number to int format
			float variableA = stof(firstNumber);

			// Calculate total
			if (buffer == 0)
				total = variableA;
			else
				total *= variableA;

			buffer = secondSpace;
			returnBuffer += returnBuffer.empty() ? firstNumber : " x " + firstNumber;
		}
		// Convert to char array
		sprintf_s(finalOutput, "%f", total);

		return returnBuffer + " = " + finalOutput;
	}
	else if (message.find("div") == 0)			// Divide command
	{
		while (true)
		{
			string firstNumber;

			size_t firstSpace = message.find(' ', buffer);
			size_t secondSpace = message.find(' ', firstSpace + 1);

			// Missing space error
			if (secondSpace == -1)
			{
				secondSpace = message.length();
				cout << "Second space error - reset" << endl;
			}
			else if (firstSpace == -1)
				break;
			else if ((firstSpace + 1) - (secondSpace - firstSpace - 1) == 0)
			{
				cout << endl << "inputted incomplete command of spaces, return empty" << endl;
				return "Unknown command: " + message; 	// echo
			}
			// Parse numbers
			firstNumber = message.substr(firstSpace + 1, secondSpace - firstSpace - 1);

			cout << "Read number: " + firstNumber << endl;

			// Space errors passed: one of the numbers are empty or not a number
			if (firstNumber == "" || !IsNumber(firstNumber))
			{
				cout << endl << "inputted numbers are not valid, return empty" << endl;
				break;
			}
			// Cannot divide by 0 error check
			else if (buffer != 0 && firstNumber == "0")
			{
				cout << "Attempted to divide by 0 : ERROR" << endl;
				return  "Attempted to divide by 0 : ERROR";
			}
			// string format number to int format
			float variableA = stof(firstNumber);

			// Calculate total
			if (buffer == 0)
				total = variableA;
			else
				total /= variableA;

			buffer = secondSpace;
			returnBuffer += returnBuffer.empty() ? firstNumber : " / " + firstNumber;
		}
		// Convert to char array
		sprintf_s(finalOutput, "%f", total);

		return returnBuffer + " = " + finalOutput;
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
	bool decimalFound = FALSE;

	while (i-- && isnum) {
		if (i == 0 && input[i] == '-') // Negative signs accepted ( only once at beginning )
			continue;
		else if (input[i] == '.') { // Decimals accepted ( only once )
			if (decimalFound)
				return false;
			else
			{
				decimalFound = true;
				continue;
			}
		}
		else if (!(input[i] >= '0' && input[i] <= '9'))
			return false;
	}
	return true;
}