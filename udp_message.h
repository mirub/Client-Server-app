#ifndef UDP_MESSAGE_H
#define UDP_MESSAGE_H

#include <bits/stdc++.h>

struct UDP_Message {
    char topic[51];
    int type;
    char value[2000];
};

#endif // UDP_MESSAGE_H