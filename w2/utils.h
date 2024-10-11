#pragma once

#include <iostream>
#include <regex>
#include <string>
#include <iterator>

#include <enet/enet.h>

void send_reliable(ENetPeer* peer, const char* msg) {
    ENetPacket* packet = enet_packet_create(msg, strlen(msg) + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
}

void send_unreliable(ENetPeer* peer, const char* msg) {
    ENetPacket* packet = enet_packet_create(msg, strlen(msg) + 1, ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 1, packet);
}

bool match_exists(std::string source, std::string regex)
{
    std::regex reg(regex);
    return std::regex_search(source, reg);
}

std::string find_match(std::string source, std::string regex, int match_group = 0)
{
    std::regex reg(regex);
    auto words_begin = std::sregex_iterator(source.begin(), source.end(), reg);
    auto words_end = std::sregex_iterator();
    if (words_begin != words_end)
    {
        std::smatch match = *words_begin;
        return match[match_group].str();
    }
    return "";
}