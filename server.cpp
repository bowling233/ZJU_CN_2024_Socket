#include "packet.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <ctime>
#include <iostream>
#include <map>
#include <mutex>
#include <thread>

std::map<int, sockaddr_in> clients;
std::mutex clientsMutex;
const int SERVER_PORT = 6026;

void handleClient(int clientSocket)
{
	std::string helloMessage = "Hello from server";
	Packet helloPacket = createPacket(INDICATION_MESSAGE, helloMessage);
	std::vector<uint8_t> serializedHello = serializePacket(helloPacket);
	send(clientSocket, serializedHello.data(), serializedHello.size(), 0);

	while (true) {
		std::vector<uint8_t> buffer(1024);
		int bytesReceived =
			recv(clientSocket, buffer.data(), buffer.size(), 0);
		if (bytesReceived <= 0) {
			break;
		}

		Packet receivedPacket = deserializePacket(buffer);
		Packet responsePacket;

		switch (receivedPacket.type) {
		case REQUEST_TIME: {
			std::time_t currentTime = std::time(nullptr);
			std::string timeStr = std::ctime(&currentTime);
			responsePacket = createPacket(RESPONSE_TIME, timeStr);
			break;
		}
		case REQUEST_NAME: {
			std::string serverName = "Server";
			responsePacket =
				createPacket(RESPONSE_NAME, serverName);
			break;
		}
		case REQUEST_CLIENT_LIST: {
			std::lock_guard<std::mutex> lock(clientsMutex);
			std::string clientList;
			for (const auto &client : clients) {
				clientList +=
					"Client " +
					std::to_string(client.first) + ": " +
					inet_ntoa(client.second.sin_addr) +
					":" +
					std::to_string(
						ntohs(client.second.sin_port)) +
					"\n";
			}
			responsePacket =
				createPacket(RESPONSE_CLIENT_LIST, clientList);
			break;
		}
		case REQUEST_SEND_MESSAGE: {
			int targetClientId = receivedPacket.data[0];
			std::string message(receivedPacket.data.begin() + 1,
					    receivedPacket.data.end());

			std::lock_guard<std::mutex> lock(clientsMutex);
			if (clients.find(targetClientId) != clients.end()) {
				int targetSocket = targetClientId;
				Packet indicationPacket = createPacket(
					INDICATION_MESSAGE, message);
				std::vector<uint8_t> serializedIndication =
					serializePacket(indicationPacket);
				send(targetSocket, serializedIndication.data(),
				     serializedIndication.size(), 0);

				responsePacket = createPacket(
					RESPONSE_SEND_MESSAGE,
					"Message sent successfully");
			} else {
				responsePacket =
					createPacket(RESPONSE_SEND_MESSAGE,
						     "Client not found");
			}
			break;
		}
		default:
			continue;
		}

		std::vector<uint8_t> serializedResponse =
			serializePacket(responsePacket);
		send(clientSocket, serializedResponse.data(),
		     serializedResponse.size(), 0);
	}

	close(clientSocket);
	std::lock_guard<std::mutex> lock(clientsMutex);
	clients.erase(clientSocket);
}

int main()
{
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		std::cerr << "Failed to create socket" << std::endl;
		return 1;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(SERVER_PORT);

	if (bind(serverSocket, (sockaddr *)&serverAddr, sizeof(serverAddr)) ==
	    -1) {
		std::cerr << "Failed to bind socket" << std::endl;
		return 1;
	}

	if (listen(serverSocket, 10) == -1) {
		std::cerr << "Failed to listen on socket" << std::endl;
		return 1;
	}

	std::cout << "Server is listening on port " << SERVER_PORT << std::endl;

	while (true) {
		sockaddr_in clientAddr;
		socklen_t clientAddrSize = sizeof(clientAddr);
		int clientSocket = accept(serverSocket, (sockaddr *)&clientAddr,
					  &clientAddrSize);
		if (clientSocket == -1) {
			std::cerr << "Failed to accept client connection"
				  << std::endl;
			continue;
		}

		std::lock_guard<std::mutex> lock(clientsMutex);
		clients[clientSocket] = clientAddr;

		std::thread clientThread(handleClient, clientSocket);
		clientThread.detach();
	}

	close(serverSocket);
	return 0;
}