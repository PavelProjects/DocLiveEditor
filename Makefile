all: client server

client: client.cpp
	g++ client.cpp -o client -lzmq

server: server.cpp
	g++ server.cpp -o server -lzmq -lpthread