all: client server

client: client.cpp
	g++ -o client client.cpp packet.cpp -g

server: server.cpp
	g++ -o server server.cpp packet.cpp -g

clean:
	rm -f client server

run:
	./server
	./client