
all: server

LIBSRC = ${wildcard *.cc}
LIBOBJ = ${patsubst %.cc, %.o, $(LIBSRC)}

server: server.cpp $(LIBOBJ)
	g++ -std=c++11 server.cpp -lmosquitto -lpthread -lm -lSDL2 -o server

$(LIBOBJ):%.o : %.cc
	g++ -c $^ -o $@  $(ALLFLAGS) $(C++FLAGS)


