all: client server

client: client.cpp Message.hpp MyText.hpp
	g++ client.cpp -o client -lzmq -lpthread -lncurses

server: server.cpp MyText.hpp
	g++ server.cpp -o server -lzmq -lpthread