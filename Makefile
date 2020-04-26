CC = g++
CXXFLAGS = -Wall -g -std=c++11

SERV = server.cpp
CLI = subscriber.cpp

PORT = 4321

IP_SERVER = 127.0.0.1

build: server.cpp subscriber.cpp helpers.h client.h udp_message.h notification.h functions.h
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