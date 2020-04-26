#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include "client.h"

bool interpretExitCommand(char* buffer, int sockfd, Notification &notif) {
	bool toStop = false;
	if (strncmp(buffer, "exit", 4) == 0) {
		// Create the notification fields
		strcpy(notif.type, "exit");
		int n = send(sockfd, &notif, sizeof(Notification), 0);
		DIE(n < 0, "Could not send exit error.\n");
		toStop = true;
	}
	return toStop;
}

void setServerAddress(struct sockaddr_in &serverAddr, int port, char* address, sa_family_t family)  {
	// Set the server address fields
    serverAddr.sin_family = family;
	serverAddr.sin_port = htons(port);
	int ret = inet_aton(address, &serverAddr.sin_addr);
    DIE(ret == 0, "Could not set server address.\n");
}

void interpretSubscribeCommand(char* buffer, int sockfd, Notification &notif) {
	// Send message to server
	char *type = strtok(buffer, " \n");
	char *topic = strtok(NULL, " \n");
	char *sf = strtok(NULL, " \n");

	strcpy(notif.type, type);	
	strcpy(notif.topic, topic);
	notif.sf = (*(sf) == '0') ? 0 : 1;

	int n = send(sockfd, &notif, sizeof(Notification), 0);
    DIE(n < 0, "Could not interpret subscribe command.\n");
	std::cout << "subscribe " << topic << std::endl;
}

void interpretUnsubscribeCommand(char* buffer, int sockfd, Notification &notif) {
	// Send message to server
	char *type = strtok(buffer, " \n");
	char *topic = strtok(NULL, " \n");
	strcpy(notif.type, type);
	strcpy(notif.topic, topic);

	int n = send(sockfd, &notif, sizeof(Notification), 0);
    DIE(n < 0, "Could not interpret subscribe commandun.\n");
	std::cout << "unsubscribe " << topic << std::endl;
}

void sendExitCommand(int fdMax, fd_set readFDs, int listenUDP, int listenTCP,
	UDP_Message &msg) {
	for (int j = 0; j <= fdMax; ++j) {
		if (j != STDIN_FILENO && FD_ISSET(j, &readFDs) &&
			j != listenUDP && j != listenTCP) {
			int n = send(j, &msg, sizeof(UDP_Message), 0);
			DIE(n < 0, "Could not send exit message to clients.");
		}	
	}
}

client newTCPclient (int newsockfd, char* buffer) {
	client tcpClient;
	memset(&tcpClient, 0, sizeof(client));
	tcpClient.cliFD = newsockfd;
	tcpClient.online = true;
	memset(tcpClient.clientID, 0, 12);
	strcpy(tcpClient.clientID, buffer);
	tcpClient.pendingMessages = std::queue<UDP_Message>();
	tcpClient.sfTopic = std::map<std::string, int>();
	return tcpClient;
}

void notifyExistingClients(int fdMax, int listenTCP, int listenUDP, 
    int newsockfd, fd_set readFDs) {
	for (int j = 0; j <= fdMax; ++j) {
        if (j != listenTCP && j != STDIN_FILENO && 
            j != listenUDP && FD_ISSET(j, &readFDs)) {
            UDP_Message msg;
            memset(&msg, 0, sizeof(UDP_Message));
            memset(msg.topic, 0, sizeof(msg.topic));
            strcpy(msg.topic, "notif");

            char message_to_send[BUFLEN + 20] = "New client ";
            char src = newsockfd + 48;
            message_to_send[11] = src;
            memset(msg.value, 0, sizeof(msg.value));
            strcpy(msg.value, message_to_send);

            int n = send(j, &msg, sizeof(msg), 0);
            DIE (n < 0, "Could not notify existing clients.");
        }
    }
}

void notifyNewClient(int fdMax, int listenTCP, int listenUDP, 
    int newsockfd, fd_set readFDs) {
	for (int j = 0; j <= fdMax; ++j) {
		if (j != listenTCP && j != STDIN_FILENO && 
            j != listenUDP && FD_ISSET(j, &readFDs)) {
			UDP_Message msg;
			memset(&msg, 0, sizeof(UDP_Message));
			memset(msg.topic, 0, sizeof(msg.topic));
			strcpy(msg.topic, "notif");

			char message_to_send[BUFLEN + 20] = "Existing client ";
			char src = j + 48;
			message_to_send[16] = src;
			memset(msg.value, 0, sizeof(msg.value));
			strcpy(msg.value, message_to_send);

			int n = send(newsockfd, &msg, sizeof(msg), 0);
			DIE(n < 0, "Could not notify new client.");
		}
	}
}

UDP_Message parseMessage(struct sockaddr_in clientAddr, char* buffer) {
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
	return udpMessage;
}