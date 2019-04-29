
all: server

server: server.cpp
	g++ -std=c++11 server.cpp -lmosquitto -lpthread -lm -lSDL2 -o server
