//headers necessary for WinSock
#include <winsock2.h>
#include <WS2tcpip.h>

#include <iostream>
#include <string>

//this is a nice way of adding a library to the linker's input without fussing with the project's settings
#pragma comment(lib, "Ws2_32.lib")

using namespace std;

const int SERVER_TCP_PORT = 8080;

//the server and client need to be set up differently
int proceedServer(WSADATA&, int, char**);
int proceedClient(WSADATA&, int, char**);

int main(int argc, char** argv) {
	WSADATA wsa_data;
	cout << "Will you be the making the connection (client) or waiting for the connection (server)?" << endl;
	cout << "0: Client" << endl;
	cout << "1: Server" << endl;
	
	int response;
	cin >> response;

	//Initialize the WinSock library using version 2.0
	if(WSAStartup(MAKEWORD(2, 0), &wsa_data) != 0) {
		cerr << "Error while initializing socket library: " << WSAGetLastError() << endl;
		return 1;
	}

	int retVal;
	if(response) {
		retVal = proceedServer(wsa_data, argc, argv);
	}
	else {
		retVal = proceedClient(wsa_data, argc, argv);
	}
	WSACleanup();
	system("PAUSE");
	return retVal;
}

int proceedClient(WSADATA& wsa_data, int argc, char** argv) {

	//Imagine that all the ports are like ethernet ports in a router, and a socket is the plug of a cable
	//it's unfortunate naming but that's how it is...

	//so first we need to make the plug (create a socket), then connect it (bind the socket to a port)
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);	//here we get a socket handle by using the socket(..) function call
														//nothing's special about the socket compared to the client version, yet
	if(clientSocket == -1) {
		cerr << "Error while opening client socket: " << WSAGetLastError() << endl;
		return 1;
	}

	//low-level libraries and things which use drivers and hardware interfaces often return integers to be used as "handles."
	//When you use function calls, you give the handle of whatever system resource (like a socket) you're using.
	//The important take-away is that sockets are not actually ints, but rather the int is referencing a socket -- it's really no different than a pointer vs memory at the address!
	
	//note we're not explicitly binding the socket to a port.  That's because we don't care what port is used, so this method of creating a socket will grab an ephemeral port for us
	//now we check to see if the socket was successfully created

	char ipaddr[20];
	cout << "What is the IP address of the other computer? ";
	cin >> ipaddr;

	sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(sockaddr_in));	//this struct is going to be a convenient way of bit-stuffing, so we first set its memory contents to 0
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(SERVER_TCP_PORT);	//htons does some bit-swapping...it's best to just accept it for now

	//at this point, we're basically ready on this end, now we need to tell the library to actually try to connect to another computer.
	//The necessary information about the other computer is put into a struct
	
	//inet_pton converts a "standard" IP address into the binary version, which could have an error based on the protocols in use and user input so we need to check for that
	if(inet_pton(AF_INET, ipaddr, &client_addr.sin_addr) == 0) {
		cerr << "Error while setting server's IP address: " << WSAGetLastError() << endl;
		return 1;
	}

	//we now have a socket and the structure filled in for where to connect, so time to connect
	//if connect returns 0, there was an error
	if(connect(clientSocket, (sockaddr*) &client_addr, sizeof(sockaddr_in)) != 0) {
		cerr << "Error while connecting to the server: " << WSAGetLastError() << endl;
		return 1;
	}

	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));
	socklen_t addr_len = sizeof(sockaddr_in);

	int comm_socket = accept(clientSocket, (sockaddr*)&server_addr, &addr_len);
	if(comm_socket < 0) {
		cerr << "Error accepting client connection: " << WSAGetLastError() << endl;
		closesocket(clientSocket);
		return 1;
	}
	else {
		cout << "Connected to: " << inet_ntoa(server_addr.sin_addr) << " at TCP port " << ntohs(server_addr.sin_port) << endl;
		closesocket(clientSocket);	//for this simple application, we're closing the server socket now
									//in a more involved server, we would leave the server socket open and listening for more connections
									//each connection would spawn its own socket, leaving the server socket always available for new connections
	}

	//if you've survived to this point, you now have a network connection from this computer to another!
	char recvBuffer[512];	//don't send messages more than 511 characters long!
	string sendMessage;
	int commResult;

	while(1) {
		
		//receive
		memset(recvBuffer, 0, sizeof(recvBuffer));

		commResult = recv(comm_socket, recvBuffer, 512, 0);
		if(commResult > 0) {
			cout << "\nMessage received: " << recvBuffer << endl;
		}
		else if (commResult == 0) {
			cout << "Connection closed by client." << endl;
			commResult = shutdown(comm_socket, SD_SEND);
			
			if(commResult == SOCKET_ERROR) {
				cerr << "Error while shutting down the socket: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
			else {
				closesocket(comm_socket);
				break;
			}
		}
		else {
			cerr << "Receive failed: " << WSAGetLastError() << endl;
			closesocket(comm_socket);
			return 1;
		}
		
		//send
		cout << "Message to send, or just hit [enter] twice to quit: ";
		cin.ignore();
		getline(cin, sendMessage);

		if(sendMessage.length() != 0) {
			commResult = send(comm_socket, sendMessage.c_str(), (int)strlen(sendMessage.c_str()), 0);
			
			if(commResult == SOCKET_ERROR) {
				cerr << "send failed: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
		}
		else {
			commResult = shutdown(comm_socket, SD_SEND);
			if(commResult == SOCKET_ERROR) {
				cerr << "Error while shutting down the socket: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
			break;
		}
	}

	closesocket(comm_socket);

	//close the socket before returning
	return 0;
}

int proceedServer(WSADATA& wsa_data, int argc, char** argv) {
	//setup the server socket
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);	//here we get a socket handle by using the socket(..) function call
														//nothing's special about the socket compared to the client version, yet
	
	//now we check to see if the socket was successfully created
	if(serverSocket == -1) {
		cerr << "Error while opening server socket: " << WSAGetLastError() << endl;
		return 1;
	}

	//The necessary information about the other computer is put into a struct
	sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(sockaddr_in));	//this struct is going to be a convenient way of bit-stuffing, so we first set its memory contents to 0
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_TCP_PORT);	//htons does some bit-swapping...it's best to just accept it for now
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //we don't care about the IP address of the connection; we'll find out when someone connects

	//before, we used connect, but this is a server socket!  It's not enough to know what port number we want, we have to bind to it
	if(bind(serverSocket, (sockaddr*) &server_addr, sizeof(sockaddr_in)) != 0) {
		cerr << "Error while binding the socket to the port: " << WSAGetLastError() << endl;
		return 1;
	}

	//you now have a socket attached to a port, now we have it listen for another computer trying to connect
	if(listen(serverSocket, 512) != 0) {
		cerr << "Error while setting the socket to listen: " << WSAGetLastError() << endl;
		return 1;
	}
	else {
		cout << "Waiting for client connection..." << endl;
	}

	//the socket is listening, and we must explicitly accept a connection on the port,
	//which involves creating another socket for the specific connection.
	sockaddr_in client_addr;
	memset(&client_addr, 0, sizeof(sockaddr_in));
	socklen_t addr_len = sizeof(sockaddr_in);

	int comm_socket = accept(serverSocket, (sockaddr*)&client_addr, &addr_len);
	if(comm_socket < 0) {
		cerr << "Error accepting client connection: " << WSAGetLastError() << endl;
		closesocket(serverSocket);
		return 1;
	}
	else {
		cout << "Connected to: " << inet_ntoa(client_addr.sin_addr) << " at TCP port " << ntohs(client_addr.sin_port) << endl;
		closesocket(serverSocket);	//for this simple application, we're closing the server socket now
									//in a more involved server, we would leave the server socket open and listening for more connections
									//each connection would spawn its own socket, leaving the server socket always available for new connections
	}

	char recvBuffer[512];	//don't send messages more than 511 characters long! (arbitrary limit)
	string sendMessage;
	int commResult;

	while(1) {
		
		//receive
		memset(recvBuffer, 0, sizeof(recvBuffer));

		commResult = recv(comm_socket, recvBuffer, 512, 0);
		if(commResult > 0) {
			cout << "\nMessage received: " << recvBuffer << endl;
		}
		else if (commResult == 0) {
			cout << "Connection closed by client." << endl;
			commResult = shutdown(comm_socket, SD_SEND);
			
			if(commResult == SOCKET_ERROR) {
				cerr << "Error while shutting down the socket: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
			else {
				closesocket(comm_socket);
				break;
			}
		}
		else {
			cerr << "Receive failed: " << WSAGetLastError() << endl;
			closesocket(comm_socket);
			return 1;
		}
		
		//send
		cout << "Message to send, or just hit [enter] twice to quit: ";
		cin.ignore();
		getline(cin, sendMessage);

		if(sendMessage.length() != 0) {
			commResult = send(comm_socket, sendMessage.c_str(), (int)strlen(sendMessage.c_str()), 0);
			
			if(commResult == SOCKET_ERROR) {
				cerr << "send failed: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
		}
		else {
			commResult = shutdown(comm_socket, SD_SEND);
			if(commResult == SOCKET_ERROR) {
				cerr << "Error while shutting down the socket: " << WSAGetLastError() << endl;
				closesocket(comm_socket);
				return 1;
			}
			break;
		}
	}

	closesocket(comm_socket);
	return 0;
}