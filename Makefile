
all: server

server: server.cpp
	g++ server.cpp -lmosquitto -lpthread -lm -o server
