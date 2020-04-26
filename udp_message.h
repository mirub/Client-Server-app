#ifndef UDP_MESSAGE_H
#define UDP_MESSAGE_H

#include <bits/stdc++.h>

struct UDP_Message {
    char topic[51];
    char type[15];
    char value[2000];
    char udpIP[20];
    char udpPort[10];
};

#endif // UDP_MESSAGE_H