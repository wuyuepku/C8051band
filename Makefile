
all: server

server: server.cpp
	g++ server.cpp -lmosquitto -lpthread -lm -lSDL2 -o server
