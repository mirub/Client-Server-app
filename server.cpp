#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include "helpers.h"
#include "udp_message.h"
#include "client.h"
#include "notification.h"
#include "functions.h"

int main(int argc, char *argv[]) {
    int listenTCP, listenUDP, newsockfd, portTCP, portUDP;
	char buffer[BUFLEN];
	struct sockaddr_in serverAddrTCP, serverAddrUDP, clientAddr;
    int n = 0, i, ret;
	socklen_t cliLen;

	// Clients map
	std::unordered_map<std::string, client> tcpClients;

	// Holds the topics and the respective clients subscribed
	std::unordered_map<std::string, std::vector<client>> topics;

	// Existing file descriptors
    fd_set readFDs;

    // Auxiliary fd set
	fd_set auxFDs;

    // Max value fd out of readFDs set
	int fdMax;

    // Empty readFDs set and the temporary set auxFDs
	FD_ZERO(&readFDs);
	FD_ZERO(&auxFDs);

	// Sets the bit for the stdin file descriptor fd in readFDs.
    FD_SET(STDIN_FILENO, &readFDs);

	// Create the socket for the TCP client
    listenTCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(listenTCP < 0, "Could not create a TCP socket.");

	// Create the socket for the UDP client
	listenUDP = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(listenUDP < 0, "Could not create a UDP socket.");

	portTCP = portUDP = atoi(argv[1]);
	DIE(portTCP == 0, "Wrong port");

    // Set the server address for TCP
    memset((char *) &serverAddrTCP, 0, sizeof(serverAddrTCP));
	serverAddrTCP.sin_family = AF_INET;
	serverAddrTCP.sin_port = htons(portTCP);
	serverAddrTCP.sin_addr.s_addr = INADDR_ANY;

	// Set the server address for UDP
    memset((char *) &serverAddrUDP, 0, sizeof(serverAddrUDP));
	serverAddrUDP.sin_family = PF_INET;
	serverAddrUDP.sin_port = htons(portUDP);
	serverAddrUDP.sin_addr.s_addr = INADDR_ANY;

	// Binds the server address to the listenTCP file descriptor
    ret = bind(listenTCP, (struct sockaddr *) &serverAddrTCP,
		sizeof(struct sockaddr));
	DIE(ret < 0, "Could not bind the TCP address.");

	// Binds the server address to the listenUDP file descriptor
    ret = bind(listenUDP, (struct sockaddr *) &serverAddrUDP,
		sizeof(struct sockaddr));
	DIE(ret < 0, "Could not bind the UDP address.");

	// The server socket waits for the client to approach
	// the server to make a connection
	ret = listen(listenTCP, MAX_CLIENTS);
	DIE(ret < 0, "Could not make a connection to any TCP client.");

	// Add the new fd to the readFDs set
	FD_SET(listenTCP, &readFDs);

	// Add the new fd to the readFDs set
	FD_SET(listenUDP, &readFDs);

	fdMax = listenTCP > listenUDP ? listenTCP : listenUDP;
	bool toStop = false;

	while(!toStop) {
		auxFDs = readFDs;
		memset(buffer, 0, BUFLEN);

		// Wait for an action on one of the sockets 
		ret = select(fdMax + 1, &auxFDs, NULL, NULL, NULL);
		DIE(ret < 0, "Could not select socket. \n");

		// Loop through the file descriptors
		for (i = 0; i <= fdMax; i++) {
			if (FD_ISSET(i, &auxFDs)) {
				if (i == STDIN_FILENO) {
					// Receive input from stdin
					memset(buffer, 0, BUFLEN);
					char* read = fgets(buffer, BUFLEN - 1, stdin);
					DIE(read == nullptr, "Could not read from stdin.");

					// If the command is exit
					if (strncmp(buffer, "exit", 4) == 0) {
						UDP_Message msg;
						memset(&msg, 0, sizeof(UDP_Message));
						strcpy(msg.topic, "exit");
						sendExitCommand(fdMax, readFDs, listenUDP, listenTCP, msg);
						toStop = true;
						break;
					}
					DIE(strncmp(buffer, "exit", 4) != 0, "Wrong keyboard command for server.\n");
					
				} else if (i == listenTCP) {
					// New connection on thge inactive server (via listen)
					// which the server accepts
					cliLen = sizeof(clientAddr);

					// Extracts the first connection request on the queue of pending
					// connections for the listening socket
					newsockfd = accept(listenTCP, (struct sockaddr *) &clientAddr, &cliLen);
					DIE(newsockfd < 0, "Could not find connection.");
					
					int enable = 1;
					// Turn off Neagle algorithm
					n = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
					DIE(n < 0, "Could not turn off the Neagle algorithm.");

					// Adds the socket returned by accept to the readFDs set
					FD_SET(newsockfd, &readFDs);

					if (newsockfd > fdMax) { 
						fdMax = newsockfd;
					}

					// Notify the new client of the existing clients
					notifyNewClient(fdMax, listenTCP, listenUDP, newsockfd, readFDs);

					// Notify the existing clients of the new client
					notifyExistingClients(fdMax, listenTCP, listenUDP, newsockfd, readFDs);

					// Receive the ID
					memset(buffer, 0, BUFLEN);
					n = recv(newsockfd, buffer, sizeof(buffer), 0);
					DIE(n < 0, "Could not receive the client ID.");

					// Search if the client already exists
					auto it = tcpClients.find(buffer);
					if (it != tcpClients.end()) {
						tcpClients[buffer].online = true;
						tcpClients[buffer].cliFD = newsockfd;
						while (!tcpClients[buffer].pendingMessages.empty()) {
							n = send(newsockfd, &tcpClients[buffer].pendingMessages.front(), sizeof(UDP_Message), 0);
							DIE(n < 0, "Could not send enqueued message.");
							tcpClients[buffer].pendingMessages.pop();
						}

					} else {
						// Create new TCP client entry
						client tcpClient = newTCPclient(newsockfd, buffer);

 						// Adds the new client to the map
						tcpClients.insert({tcpClient.clientID, tcpClient});

						std::cout<< "New client " << tcpClient.clientID << " connected from ";
						std::cout << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "\n";
					}

				} else if (i == listenUDP) {
					cliLen = sizeof(clientAddr);
					// Receive the data from the UDP clients
					memset(buffer, 0, BUFLEN);

					// Receive the message
					n = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr*) &clientAddr ,&cliLen);
					DIE(n < 0, "UDP receive");

					// Parse the message
					UDP_Message udpMessage = parseMessage(clientAddr, buffer);

					for (unsigned int k = 0; k < topics[udpMessage.topic].size();
						++k) {
						if (tcpClients[topics[udpMessage.topic][k].clientID].online ==
						true) {
							send(tcpClients[topics[udpMessage.topic][k].clientID].cliFD,
								&udpMessage, sizeof(UDP_Message), 0);

						} else if (tcpClients[topics[udpMessage.topic][k].clientID].sfTopic[udpMessage.topic]
							== 1 && tcpClients[topics[udpMessage.topic][k].clientID].online == false) {

							tcpClients[topics[udpMessage.topic][k].clientID].pendingMessages.push(udpMessage);
						}
					}
				} else {	
					// Received data on one of the TCP client sockets
					Notification notif;
					memset(&notif, 0, sizeof(Notification));
					n = recv(i, &notif, sizeof(Notification), 0);
					DIE(n < 0, "recv");

					if (strncmp(notif.type, "exit", 4) == 0) {
						std::cout << "Client " << notif.clientId << " disconnected.\n";
						tcpClients[notif.clientId].online = false;
					} else if (strncmp(notif.type, "subscribe", 9) == 0) {
						// Search topic
						auto it = topics.find(notif.topic);
						int newConnection = 0;

						// Add the topic to the topics map
						if (it == topics.end()) {
							std::vector<client> cliArr;
							newConnection = 1;
							cliArr.push_back(tcpClients[notif.clientId]);
							topics.insert({notif.topic, cliArr});
						}						

						for (unsigned int k = 0; k < topics[notif.topic].size(); ++k) {
							if (strcmp(topics[notif.topic][k].clientID, notif.clientId) == 0 &&
								newConnection == 0) {
							} else if (!newConnection){
								topics[notif.topic].push_back(tcpClients[notif.clientId]);
							}
						}

						tcpClients[notif.clientId].sfTopic.insert({notif.topic, notif.sf});
					} else if (strncmp(notif.type, "unsubscribe", 11) == 0) {
						for (auto it = topics[notif.topic].begin(); it != topics[notif.topic].end(); ++it) {
							if (strcmp(it->clientID, notif.clientId) == 0) {
								topics[notif.topic].erase(it);
								break;
							}
						}
					} else {
						DIE(!((strncmp(notif.type, "exit", 4) == 0) || (strncmp(notif.type,
							"subscribe", 9) == 0) || (strncmp(notif.type, "unsubscribe", 11) == 0)),
							"Invalid command received.");
					}

					if (n == 0) {
						// Connection closed
						close(i);
						
						// Removed the closed socket
						FD_CLR(i, &readFDs);
					}
				}
			}
		}
	}

	for (int i = 0; i <= fdMax; ++i) {
		if (FD_ISSET(i, &auxFDs)) {
			close(i);
		}
	}

    return 0;
}