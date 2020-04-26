#ifndef CLIENT_H
#define CLIENT_H

#include <bits/stdc++.h>
#include "udp_message.h"

struct client {
    // The client ID
    char clientID[10];
    // The file descriptor of the client
    int cliFD;
    // State of the user
    bool online;
    // Messages that had not been sent due to
    // disconnection
    std::queue<UDP_Message> pendingMessages;
    // The SF state of each topic 
    std::map<std::string, int> sfTopic;

    client() {}

    client(int new_cliFD, bool new_online, std::map<std::string, int> new_sfTopic,
        std::queue<UDP_Message> new_pendingMessages, char* new_clientID) {
            strcpy(clientID, new_clientID);
            cliFD = new_cliFD;
            online = new_online;
            sfTopic = new_sfTopic;
            pendingMessages = new_pendingMessages;
    }
};

#endif // CLIENT_H