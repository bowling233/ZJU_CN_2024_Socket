#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <vector>
#include <string>

enum PacketType {
        REQUEST_TIME = 1,
        REQUEST_NAME,
        REQUEST_CLIENT_LIST,
        REQUEST_SEND_MESSAGE,
        RESPONSE_TIME,
        RESPONSE_NAME,
        RESPONSE_CLIENT_LIST,
        RESPONSE_SEND_MESSAGE,
        INDICATION_MESSAGE
};

struct Packet {
        uint32_t length;
        PacketType type;
        std::vector<uint8_t> data;

        Packet()
                : length(0)
                , type(REQUEST_TIME)
        {
        }
};

std::vector<uint8_t> serializePacket(const Packet &packet);
Packet deserializePacket(const std::vector<uint8_t> &buffer);
Packet createPacket(PacketType type, const std::vector<uint8_t> &data);
Packet createPacket(PacketType type, const std::string &data);

#endif // PACKET_H