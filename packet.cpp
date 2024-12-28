#include "packet.h"
#include <cstring>

std::vector<uint8_t> serializePacket(const Packet &packet)
{
        std::vector<uint8_t> buffer;
        buffer.resize(sizeof(packet.length) + sizeof(packet.type) +
                          packet.data.size());

        uint8_t *ptr = buffer.data();
        memcpy(ptr, &packet.length, sizeof(packet.length));
        ptr += sizeof(packet.length);
        memcpy(ptr, &packet.type, sizeof(packet.type));
        ptr += sizeof(packet.type);
        memcpy(ptr, packet.data.data(), packet.data.size());

        return buffer;
}

Packet deserializePacket(const std::vector<uint8_t> &buffer)
{
        Packet packet;
        const uint8_t *ptr = buffer.data();

        memcpy(&packet.length, ptr, sizeof(packet.length));
        ptr += sizeof(packet.length);
        memcpy(&packet.type, ptr, sizeof(packet.type));
        ptr += sizeof(packet.type);
        packet.data.assign(ptr, ptr + packet.length - sizeof(packet.length) -
                                        sizeof(packet.type));

        return packet;
}

Packet createPacket(PacketType type, const std::vector<uint8_t> &data)
{
        Packet packet;
        packet.type = type;
        packet.data = data;
        packet.length =
                sizeof(packet.length) + sizeof(packet.type) + data.size();
        return packet;
}

Packet createPacket(PacketType type, const std::string &data)
{
        std::vector<uint8_t> dataVec(data.begin(), data.end());
        return createPacket(type, dataVec);
}