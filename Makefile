all: client server

client: client.cpp
	g++ client.cpp -o client -lzmq -lpthread

server: server.cpp
	g++ server.cpp -o server -lzmq -lpthread