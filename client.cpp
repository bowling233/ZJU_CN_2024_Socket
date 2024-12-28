#include "packet.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <fcntl.h>
#include <sys/select.h>
#include <cstring>

std::mutex queueMutex;
std::condition_variable cv;
std::queue<Packet> messageQueue;
bool connected = false;
int clientSocket = -1;
std::thread receiveThread;
std::atomic<bool> exitFlag(false);

void receiveData()
{
	while (connected) {
		std::vector<uint8_t> buffer(1024);
		int bytesReceived =
			recv(clientSocket, buffer.data(), buffer.size(), 0);
		if (bytesReceived <= 0) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				std::this_thread::sleep_for(
					std::chrono::milliseconds(100));
				continue;
			} else {
				connected = false;
				break;
			}
		} else if (bytesReceived == 0) {
			connected = false;
			break;
		}

		Packet receivedPacket = deserializePacket(buffer);
		{
			std::lock_guard<std::mutex> lock(queueMutex);
			messageQueue.push(receivedPacket);
		}
		cv.notify_one();
	}
}

void handleMessages()
{
	while (true) {
		std::unique_lock<std::mutex> lock(queueMutex);
		cv.wait(lock, [] {
			return !messageQueue.empty() || exitFlag.load();
		});

		if (exitFlag.load() && messageQueue.empty()) {
			break;
		}

		if (messageQueue.empty()) {
			continue;
		}

		Packet packet = messageQueue.front();
		messageQueue.pop();
		lock.unlock();

		switch (packet.type) {
		case RESPONSE_TIME:
			std::cout << "Server Time: "
				  << std::string(packet.data.begin(),
						 packet.data.end())
				  << std::endl;
			break;
		case RESPONSE_NAME:
			std::cout << "Server Name: "
				  << std::string(packet.data.begin(),
						 packet.data.end())
				  << std::endl;
			break;
		case RESPONSE_CLIENT_LIST:
			std::cout << "Client List: "
				  << std::string(packet.data.begin(),
						 packet.data.end())
				  << std::endl;
			break;
		case RESPONSE_SEND_MESSAGE:
			std::cout << "Message Response: "
				  << std::string(packet.data.begin(),
						 packet.data.end())
				  << std::endl;
			break;
		case INDICATION_MESSAGE:
			std::cout << "Message from Server: "
				  << std::string(packet.data.begin(),
						 packet.data.end())
				  << std::endl;
			break;
		default:
			std::cerr << "Unknown packet type received."
				  << std::endl;
			break;
		}
	}
}

void setSocketNonBlocking(int socket)
{
	int flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1) {
		std::cerr << "Failed to get socket flags" << std::endl;
		return;
	}
	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
		std::cerr << "Failed to set socket to non-blocking"
			  << std::endl;
	}
}

void connectToServer(const std::string &ip, int port)
{
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (clientSocket == -1) {
		std::cerr << "Failed to create socket" << std::endl;
		return;
	}

	setSocketNonBlocking(clientSocket);

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

	if (connect(clientSocket, (sockaddr *)&serverAddr,
		    sizeof(serverAddr)) == -1) {
		if (errno != EINPROGRESS) {
			std::cerr << "Failed to connect to server" << std::endl;
			close(clientSocket);
			clientSocket = -1;
			return;
		}
	}

	fd_set writefds;
	FD_ZERO(&writefds);
	FD_SET(clientSocket, &writefds);
	timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	int result = select(clientSocket + 1, NULL, &writefds, NULL, &timeout);
	if (result <= 0 || !FD_ISSET(clientSocket, &writefds)) {
		std::cerr << "Failed to connect to server" << std::endl;
		close(clientSocket);
		clientSocket = -1;
		return;
	}

	int so_error;
	socklen_t len = sizeof(so_error);
	getsockopt(clientSocket, SOL_SOCKET, SO_ERROR, &so_error, &len);
	if (so_error != 0) {
		std::cerr
			<< "Failed to connect to server: " << strerror(so_error)
			<< std::endl;
		close(clientSocket);
		clientSocket = -1;
		return;
	}

	connected = true;
	receiveThread = std::thread(receiveData);
	std::cout << "Connected to server" << std::endl;
}

void disconnectFromServer()
{
	if (!connected) {
		return;
	}
	connected = false;
	close(clientSocket);
	if (receiveThread.joinable()) {
		receiveThread.join();
	}
	std::cout << "Disconnected from server" << std::endl;
}

void sendRequest(PacketType type, const std::vector<uint8_t> &data = {})
{
	if (!connected) {
		std::cerr << "Not connected to server" << std::endl;
		return;
	}

	Packet packet = createPacket(type, data);
	std::vector<uint8_t> serializedPacket = serializePacket(packet);
	send(clientSocket, serializedPacket.data(), serializedPacket.size(), 0);
}

void sendRequest(PacketType type, const std::string &data)
{
	sendRequest(type, std::vector<uint8_t>(data.begin(), data.end()));
}

void menu()
{
	while (true) {
		std::cout << "Menu:\n"
			  << "1. Connect to server\n"
			  << "2. Disconnect from server\n"
			  << "3. Request server time\n"
			  << "4. Request server name\n"
			  << "5. Request client list\n"
			  << "6. Send message to client\n"
			  << "7. Exit\n"
			  << "Enter your choice: ";
		int choice;
		std::cin >> choice;

		switch (choice) {
		case 1: {
			std::string ip;
			int port;
			std::cout << "Enter server IP: ";
			std::cin >> ip;
			std::cout << "Enter server port: ";
			std::cin >> port;
			connectToServer(ip, port);
			break;
		}
		case 2:
			disconnectFromServer();
			break;
		case 3:
			sendRequest(REQUEST_TIME);
			break;
		case 4:
			sendRequest(REQUEST_NAME);
			break;
		case 5:
			sendRequest(REQUEST_CLIENT_LIST);
			break;
		case 6: {
			int clientId;
			std::string message;
			std::cout << "Enter client ID: ";
			std::cin >> clientId;
			std::cin.ignore();
			std::cout << "Enter message: ";
			std::getline(std::cin, message);
			std::vector<uint8_t> data = { static_cast<uint8_t>(
				clientId) };
			data.insert(data.end(), message.begin(), message.end());
			sendRequest(REQUEST_SEND_MESSAGE, data);
			break;
		}
		case 7:
			disconnectFromServer();
			exitFlag.store(true);
			cv.notify_all();
			return;
		default:
			std::cerr << "Invalid choice" << std::endl;
			break;
		}
	}
}

int main()
{
	std::thread messageHandler(handleMessages);
	menu();
	if (messageHandler.joinable()) {
		messageHandler.join();
	}
	return 0;
}