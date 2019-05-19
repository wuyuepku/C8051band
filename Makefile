
all: server

LIBSRC = ${wildcard *.cc}
LIBOBJ = ${patsubst %.cc, %.o, $(LIBSRC)}

server: server.cpp $(LIBOBJ)
	g++ -std=c++11 server.cpp $(LIBOBJ) -lmosquitto -lpthread -lm -lSDL2 -o server

$(LIBOBJ):%.o : %.cc
	g++ -c $^ -o $@  $(ALLFLAGS) $(C++FLAGS)

clean:
	rm server
	rm *.o
