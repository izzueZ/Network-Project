CFLAGS = -std=c++0x
SOURCE_FILES = shared.h server.cpp client.cpp
SUPPLEMENTARY_FILES = Makefile README report.pdf

default: server client

server: server.cpp
	g++ server.cpp -o server

client: client.cpp
	g++ client.cpp -o client

pedantic:
	g++ $(CFLAGS) -g -Wall -Wextra -pedantic client.cpp -o client
	g++ $(CFLAGS) -g -Wall -Wextra -pedantic server.cpp -o server

transfer:
	cp $(SOURCE_FILES) $(SUPPLEMENTARY_FILES) ~/VirtualBox\ VMs/VMShare

clean:
	-rm client server received.data