all: client server

client: client.cpp
	g++ -o client client.cpp packet.cpp

server: server.cpp
	g++ -o server server.cpp packet.cpp

clean:
	rm -f client server

run:
	./server
	./client