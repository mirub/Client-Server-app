#include <bits/stdc++.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include  <netinet/tcp.h>
#include "helpers.h"
#include "udp_message.h"
#include "client.h"
#include "notification.h"

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
	DIE(listenTCP < 0, "socket");

	// Create the socket for the UDP client
	listenUDP = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(listenUDP < 0, "socket");

	portTCP = atoi(argv[1]);
	DIE(portTCP == 0, "atoi");

	portUDP = atoi(argv[1]);
	DIE(portUDP == 0, "atoi");

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
    ret = bind(listenTCP, (struct sockaddr *) &serverAddrTCP, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	// Binds the server address to the listenUDP file descriptor
    ret = bind(listenUDP, (struct sockaddr *) &serverAddrUDP, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	// The server socket waits for the client to approach
	// the server to make a connection
	ret = listen(listenTCP, MAX_CLIENTS); // CHECK MAX CLIENTS
	DIE(ret < 0, "listen");

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
		DIE(ret < 0, "select");

		// Loop through the file descriptors
		for (i = 0; i <= fdMax; i++) {
			if (FD_ISSET(i, &auxFDs)) {
				if (i == STDIN_FILENO) {
					// Receive input from stdin
					memset(buffer, 0, BUFLEN);
					fgets(buffer, BUFLEN - 1, stdin);

					if (strncmp(buffer, "exit", 4) == 0) {
						UDP_Message msg;
						memset(&msg, 0, sizeof(UDP_Message));
						strcpy(msg.topic, "exit");

						for (int j = 0; j <= fdMax; ++j) {
							if (j != STDIN_FILENO && FD_ISSET(j, &readFDs) &&
								j != listenUDP && j != listenTCP) {

								// char exitMsg[5] = "exit";
								n = send(j, &msg, sizeof(UDP_Message), 0);
								DIE(n < 0, "send exit");
							}	
						}

						toStop = true;
						break;
					}

					// OTHER COMMAND IS FAIL
					
				} else if (i == listenTCP) {
					// New connection on thge inactive server (via listen)
					// which the server accepts
					cliLen = sizeof(clientAddr);

					// Extracts the first connection request on the queue of pending
					// connections for the listening socket
					newsockfd = accept(listenTCP, (struct sockaddr *) &clientAddr, &cliLen);
					DIE(newsockfd < 0, "accept");
					
					int enable = 1;
					// Turn off Neagle algorithm
					setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

					// Adds the socket returned by accept to the readFDs set
					FD_SET(newsockfd, &readFDs);

					if (newsockfd > fdMax) { 
						fdMax = newsockfd;
					}

					// Receive the ID
					memset(buffer, 0, BUFLEN);
					n = recv(newsockfd, buffer, sizeof(buffer), 0);
					// DIE

					// Search if the client already exists
					auto it = tcpClients.find(buffer);
					if (it != tcpClients.end()) {

						std::cout << "The client " << tcpClients[buffer].clientID << " exists.\n";
						tcpClients[buffer].online = true;
						tcpClients[buffer].cliFD = newsockfd;
						while (!tcpClients[buffer].pendingMessages.empty()) {
							send(newsockfd, &tcpClients[buffer].pendingMessages.front(), sizeof(UDP_Message), 0);
							tcpClients[buffer].pendingMessages.pop();
						}

					} else {
						// Create new TCP client entry
						client tcpClient;
						memset(&tcpClient, 0, sizeof(client));
						tcpClient.cliFD = newsockfd;
						tcpClient.online = true;
						memset(tcpClient.clientID, 0, 12);
						strcpy(tcpClient.clientID, buffer);
						tcpClient.pendingMessages = std::queue<UDP_Message>();
						tcpClient.sfTopic = std::map<std::string, int>();
 						// Adds the new client to the map
						tcpClients.insert({tcpClient.clientID, tcpClient});

						std::cout<< "New client " << tcpClient.clientID << " connected from ";
						std::cout << inet_ntoa(clientAddr.sin_addr) << ":" << ntohs(clientAddr.sin_port) << "\n";
					}

					// Notify the new client of the existing clients
					for (int j = 0; j < fdMax; ++j) {
						if (j != listenTCP) {
							UDP_Message msg;
							memset(&msg, 0, sizeof(UDP_Message));
							memset(msg.topic, 0, sizeof(msg.topic));
							strcpy(msg.topic, "notif");

							char message_to_send[BUFLEN + 20] = "Existing client ";
							char src = j + 48;
							message_to_send[16] = src;
							message_to_send[17] = '\n';
							memset(msg.value, 0, sizeof(msg.value));
							strcpy(msg.value, message_to_send);

							send(newsockfd, &msg, sizeof(msg), 0);
						}
					}

					// Notify the existing clients of the new client
					for (int j = 0; j < fdMax; ++j) {
						if (j != listenTCP) {
							UDP_Message msg;
							memset(&msg, 0, sizeof(UDP_Message));
							memset(msg.topic, 0, sizeof(msg.topic));
							strcpy(msg.topic, "notif");

							char message_to_send[BUFLEN + 20] = "New client ";
							char src = newsockfd + 48;
							message_to_send[11] = src;
							memset(msg.value, 0, sizeof(msg.value));
							strcpy(msg.value, message_to_send);

							send(j, &msg, sizeof(msg), 0);
						}
					}

				} else if (i == listenUDP) {
					cliLen = sizeof(clientAddr);
					// Receive the data from the UDP clients
					memset(buffer, 0, BUFLEN);

					// Receive the message
					n = recvfrom(i, buffer, sizeof(buffer), 0, (struct sockaddr*) &clientAddr ,&cliLen);
					DIE(n < 0, "UDP receive");

					// Parse the message
					UDP_Message udpMessage;
					strcpy(udpMessage.udpIP, inet_ntoa(clientAddr.sin_addr));
					sprintf(udpMessage.udpPort, "%u", (unsigned int)ntohs(clientAddr.sin_port));
					strcpy(udpMessage.topic, buffer);
					int type = *(buffer + 50);

					switch (type) {
						case 0: {
							// If it's integer
							// CHECK SIGN BYTE
							int32_t number = ntohl(*(uint32_t*)(buffer + 52));
							if (*(buffer + 51) == 1) {
								number *= -1;
							}

							int sgn = (int)number;
							char message[30];
							sprintf(udpMessage.type, "%s", "INT\0");
							sprintf(udpMessage.value, "%d", sgn);
							break;
						}

						case 1: {
							// If it's short real
							// CHECK SIGN BYTE
							float shortNum = ntohs(*(uint16_t*)(buffer + 51));
							shortNum /= (float)(100 * 1.0);
							sprintf(udpMessage.type, "%s", "SHORT_REAL\0");
							sprintf(udpMessage.value, "%.2f", shortNum);
							break;
						}

						case 2: {
							// If it's float
							// CHECK SIGN BYTE
							int sign = *(buffer + 51);
							sign = sign ? -1 : 1;
							uint32_t x = ntohl(*(uint32_t*)(buffer + 52));
							int e = (int)*(buffer + 56);
							float doubleNum = 1.0 * sign *  x * pow(10, -e);
							sprintf(udpMessage.type, "%s", "FLOAT\0");
							sprintf(udpMessage.value, "%.*f", e, doubleNum);
							break;
						}

						default:
							// Else it's string
							sprintf(udpMessage.type, "%s", "STRING\0");
							strcpy(udpMessage.value, (buffer + 51));
						break;
					}

					// SEND THE MESSAGE
					for (int k = 0; k < topics[udpMessage.topic].size(); ++k) {
						if (tcpClients[topics[udpMessage.topic][k].clientID].online == true) {
							send(tcpClients[topics[udpMessage.topic][k].clientID].cliFD, &udpMessage, sizeof(UDP_Message), 0);

						} else if (tcpClients[topics[udpMessage.topic][k].clientID].sfTopic[udpMessage.topic] == 1 &&
								tcpClients[topics[udpMessage.topic][k].clientID].online == false) {

							tcpClients[topics[udpMessage.topic][k].clientID].pendingMessages.push(udpMessage);
						}
					}

				} else {	
					// Received data on one of the TCP client sockets
					// RECEIVE THE MESSAGE WITH SUBSCRIBE
					// OR UNSUBSCRIBE

					memset(buffer, 0, BUFLEN);
					
					// Receive the message
					Notification notif;
					memset(&notif, 0, sizeof(Notification));
					n = recv(i, &notif, sizeof(Notification), 0);
					DIE(n < 0, "recv");

					if (strncmp(notif.type, "exit", 4) == 0) {
						std::cout << "Client " << notif.clientId << " disconnected.\n";
						tcpClients[notif.clientId].online = false;
					}

					if (strncmp(notif.type, "subscribe", 9) == 0) {
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

						for (int k = 0; k < topics[notif.topic].size(); ++k) {
							if (strcmp(topics[notif.topic][k].clientID, notif.clientId) == 0 &&
								newConnection == 0) {
								std::cout << "The client has already been subscribed.\n";
							} else if (!newConnection){
								topics[notif.topic].push_back(tcpClients[notif.clientId]);
							}
						}

						tcpClients[notif.clientId].sfTopic.insert({notif.topic, notif.sf});
					}

					if (strncmp(notif.type, "unsubscribe", 11) == 0) {
						int found = 0;
						auto it = topics.find(notif.topic);
						if (it == topics.end()) {
							std::cout << "Topic not found.\n";
						}	

						for (auto it = topics[notif.topic].begin(); it != topics[notif.topic].end(); ++it) {
							if (strcmp(it->clientID, notif.clientId) == 0) {
								found = 1;
								topics[notif.topic].erase(it);
								break;
							}
						}

						if (found == 0) {
							std::cout<<"The client was not subscribed to this topic.\n";
						}
					}

					if (n == 0) {
						// Connection closed
						close(i);
						
						// Removed the closed socket
						FD_CLR(i, &readFDs);

					} else {
						// Send the message received
						
						char to_forward[BUFLEN + 20] = "Message from ";
						char src = i + '0';
						to_forward[14] = src;
						strcat(to_forward, ": ");
						strcat(to_forward, buffer + 2);
						send(buffer[0] - '0', to_forward, strlen(to_forward), 0);
					}
				}
			}
		}
	}




    return 0;
}