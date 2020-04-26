#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <unistd.h>
#include  <netinet/tcp.h>
#include "notification.h"
#include "udp_message.h"
#include "helpers.h"

bool interpretExitCommand(char* buffer, int sockfd, Notification &notif) {
	bool toStop = false;
	int n = 0;
	if (strncmp(buffer, "exit", 4) == 0) {
		// Create the notification fields
		strcpy(notif.type, "exit");
		n = send(sockfd, &notif, sizeof(Notification), 0);
		DIE(send < 0, "Cannot send exit error.\n");
		toStop = true;
	}
	return toStop;
}

void setServerAddress(struct sockaddr_in &serverAddr, char* port, char* address)  {
	// Set the server address fields
    serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(port));
	int ret = inet_aton(address, &serverAddr.sin_addr);
    DIE(ret == 0, "Could not set server address.\n");
}

void interpretSubscribeCommand(char* buffer, int sockfd, Notification &notif) {
	if (strncmp(buffer, "subscribe", 9) == 0) {
		// Send message to server
		char *type = strtok(buffer, " \n");
		char *topic = strtok(NULL, " \n");
		char *sf = strtok(NULL, " \n");

		strcpy(notif.type, type);	
		strcpy(notif.topic, topic);
		notif.sf = (*(sf) == '0') ? 0 : 1;

		int n = send(sockfd, &notif, sizeof(Notification), 0);
		std::cout << "subscribe " << topic << std::endl;
	}
}

int main(int argc, char *argv[]) {
    int sockfd, n = 0, ret, action = 0;
	struct sockaddr_in serverAddr;
	char buffer[BUFLEN];

    // Existing file descriptors
    fd_set readFDs;

    // Auxiliary fd set
	fd_set auxFDs;

    // Initialize the file descriptor sets to have zero bits for all fds. 
    FD_ZERO(&readFDs);
	FD_ZERO(&auxFDs);

    // Create a TCP socket 
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Sets the bit for the stdin file descriptor fd in readFDs.
    FD_SET(STDIN_FILENO, &readFDs);

    // Sets the bit for the sockfd file descriptor fd in readFDs.
	FD_SET(sockfd, &readFDs);

    setServerAddress(serverAddr, argv[3], argv[2]);

    // Connects sockfd to the server address
    ret = connect(sockfd, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
	DIE(ret < 0, "connect");

	// Sends the client ID
	n = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
	DIE(n < 0, "Could not send the client ID.\n");

	bool toStop = false;

	int enable = 1;
	// Turn off Neagle algorithm
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));

    while (!toStop) {
		auxFDs = readFDs;

        // Wait for an action on one of the sockets 
		action = select(sockfd + 1, &auxFDs, NULL, NULL, NULL);
		DIE(action < 0, "select");

        // If there is any action from stdin
		if (FD_ISSET(STDIN_FILENO, &auxFDs)) { 
            // Read the stdin input
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			Notification notif;
			memset(&notif, 0, sizeof(Notification));
			strcpy(notif.clientId, argv[1]);

			// Interpret the exit command
			toStop = interpretExitCommand(buffer, sockfd, notif);
			
			// Interpret the subscribe command
			interpretSubscribeCommand(buffer, sockfd, notif);
			
			if (strncmp(buffer, "unsubscribe", 11) == 0) {
				// Send message to server
				char *type = strtok(buffer, " \n");
				char *topic = strtok(NULL, " \n");
				strcpy(notif.type, type);
				strcpy(notif.topic, topic);

				n = send(sockfd, &notif, sizeof(Notification), 0);
				std::cout << "unsubscribe " << topic << std::endl;
				continue;
			}

			// OTHER MESSAGE IS AN ERROR

            // If there is any action from the server
		}
		
		if (FD_ISSET(sockfd, &auxFDs)) {
            // Receive the message from the server
			UDP_Message msg;
			memset(&msg, 0, sizeof(UDP_Message));
			recv(sockfd, &msg, sizeof(UDP_Message), 0);
			if (strncmp(msg.topic, "exit", 5) == 0) {
				break;
			}

			if (strncmp(msg.topic, "notif", 5) == 0) {
				std::cout << msg.value << "\n";
			} else {
				// IP:PORT client_UDP - topic - tip_date - valoare mesaj
				std::cout << msg.udpIP << ":" << msg.udpPort << " - " <<
					msg.topic << " - " << msg.type << " - " << msg.value << "\n";
			}
		}	
	}

    // Close the socket
	close(sockfd);

    return 0;
}