CC = g++
CXXFLAGS = -Wall -g -std=c++11 -O3

SERV = server.cpp
CLI = client.cpp

PORT = 4320

IP_SERVER = 127.0.0.1

build: server.cpp client.cpp helpers.h client.h udp_message.h notification.h
	$(CC) $(CXXFLAGS) $(SERV) -o server
	$(CC) $(CXXFLAGS) $(CLI) -o subscriber

run_server: build
	./server $(PORT)

run_tcp_client: build	
	./subscriber $(CLIENT_ID) $(IP_SERVER) $(PORT)

run_udp_client:
	sudo python3 udp_client.py $(IP_SERVER) $(PORT)

clean:
	rm -f *.o server subscriber