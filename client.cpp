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

int main(int argc, char *argv[]) {
    int sockfd, n, ret, action;
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

    // Set the server address fields
    serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serverAddr.sin_addr);
    DIE(ret == 0, "inet_aton");

    // Connects sockfd to the server address
    ret = connect(sockfd, (struct sockaddr*) &serverAddr, sizeof(serverAddr));
	DIE(ret < 0, "connect");

	// Sends the client ID
	send(sockfd, argv[1], strlen(argv[1]) + 1, 0);

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
			if (strncmp(buffer, "exit", 4) == 0) {
				// Create the notification fields
				// notif.topic = "exit";

				// char exitMsg[23] = "Client ";
				// exitMsg[7] = *argv[1];
				// strcpy(exitMsg + 8, " disconnected.");

				//strcpy(notif.topic, exitMsg);
				strcpy(notif.type, "exit");

				// n = send(sockfd, exitMsg, strlen(exitMsg), 0);
				n = send(sockfd, &notif, sizeof(Notification), 0);
				//DIE
				toStop = true;
				break;
			}

			// Interpret the subscribe command
			if (strncmp(buffer, "subscribe", 9) == 0) {
				// Send message to server

				//strcpy(notif.type, "subscribe");

				char *type = strtok(buffer, " \n");
				char *topic = strtok(NULL, " \n");
				char *sf = strtok(NULL, " \n");

				strcpy(notif.type, type);	

				// // Create the topic string
				// char topic[30];
				// int k = 0;
				// int i = 10;
				// while (*(buffer + i) != ' ') {
				// 	//topic += *(buffer + i);
				// 	topic[k] = *(buffer + i);
				// 	++k;
				// 	++i;
				// }
				strcpy(notif.topic, topic);
				//i++;
				notif.sf = (*(sf) == '0') ? 0 : 1;

				n = send(sockfd, &notif, sizeof(Notification), 0);
				std::cout << "subscribe " << topic << std::endl;
				continue;
			}

			if (strncmp(buffer, "unsubscribe", 11) == 0) {
				// Send message to server
				// strcpy(notif.type, "unsubscribe");
				// char topic[30];
				// int k = 0;
				// int i = 12;
				// while (*(buffer + i) != '\0') {
				// 	topic[k] = *(buffer + i);
				// 	++i;
				// 	++k;
				// }
				char *type = strtok(buffer, " \n");
				char *topic = strtok(NULL, " \n");
				strcpy(notif.type, type);
				strcpy(notif.topic, topic);
				//i++;
				//notif.sf = (*(buffer + i) == '0') ? 0 : 1;

				n = send(sockfd, &notif, sizeof(Notification), 0);
				std::cout << "unsubscribe " << topic << std::endl;
				continue;
			}

			// OTHER MESSAGE IS AN ERROR

			// // Send the message to the server
			// n = send(sockfd, buffer, strlen(buffer), 0);
			// DIE(n < 0, "send");

            // If there is any action from the server
		}
		
		if (FD_ISSET(sockfd, &auxFDs)) {
            // Receive the message from the server
			UDP_Message msg;
			memset(&msg, 0, sizeof(UDP_Message));
			recv(sockfd, &msg, sizeof(UDP_Message), 0);
			std::cout <<"\n\n Message from server:\n" << msg.topic <<"\n";
			if (strncmp(msg.topic, "exit", 5) == 0) {
				break;
			}
		}	
	}

    // Close the socket
	close(sockfd);

    return 0;
}