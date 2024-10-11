#include <enet/enet.h>
#include <iostream>
#include <format>
#include <stdlib.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

#include "utils.h"

const uint32_t MAX_PLAYERS = 8;

std::vector<const char*> nicknames = {
  "Nazar",
  "Byaka",
  "Killer332",
  "Hitman",
  "Violet",
  "Leo",
  "Robber",
  "Kareta"
};

int generate_unique_id() {
    return rand();
}

const char* generate_unique_nickname() {
    int nick_number = ((double)rand() / RAND_MAX) * nicknames.size();

    const char* nickname = nicknames[nick_number];
    nicknames.erase(nicknames.begin() + nick_number);

    return nickname;
}

struct Player {
    int id;
    const char* nickname;
    ENetPeer* peer;
    uint32_t roundTripTime() const { return peer->roundTripTime; }
};

int main(int argc, const char** argv)
{
    srand(time(NULL));

    if (enet_initialize() != 0)
    {
        printf("Cannot init ENet");
        return 1;
    }
    ENetAddress address;

    address.host = ENET_HOST_ANY;
    address.port = 10888;

    ENetHost* server = enet_host_create(&address, MAX_PLAYERS, 2, 0, 0);

    if (!server)
    {
        printf("Cannot create ENet server\n");
        return 1;
    }

    // create players list
    std::vector<Player> players_list = { };

    uint32_t timeStart = enet_time_get();
    uint32_t lastPingSendTime = timeStart;

    while (true)
    {
        // EVENTS HANDLING
        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0)
        {
            switch (event.type)
            {
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Connection with %x:%u severed\n", event.peer->address.host, event.peer->address.port);
                if (event.peer->address.port == 10887) // prevent lobby to be regarded as player
                {
                    continue;
                }
                for (int i = 0; i < players_list.size(); i++) // this is dumb but still
                {
                    if (players_list[i].peer == event.peer)
                    {
                        players_list.erase(players_list.begin() + i);
                        break;
                    }
                }
                break;
            case ENET_EVENT_TYPE_CONNECT:
                printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
                if (event.peer->address.port == 10887) // prevent lobby to be regarded as player
                {
                    continue;
                }
                Player player;
                player.id = generate_unique_id();
                player.nickname = generate_unique_nickname();
                player.peer = event.peer;
                // send id and nickname
                {
                    const std::string msg = std::format("your_id: {}, your_nickname: {}", player.id, player.nickname);
                    send_reliable(event.peer, msg.c_str());
                }

                // send players list
                {
                    std::string msg = "other players:\n";
                    for (const auto& player : players_list)
                    {
                        msg += std::format("id: {}, nickname: {}\n", player.id, player.nickname);
                    }
                    send_reliable(event.peer, msg.c_str());
                }

                players_list.push_back(player);

                break;
            case ENET_EVENT_TYPE_RECEIVE:
                printf("Packet received '%s'\n", event.packet->data);
                enet_packet_destroy(event.packet);
                break;
            default:
                break;
            };
        }

        // OTHER STUFF
        uint32_t curTime = enet_time_get();
        if (curTime - lastPingSendTime > 1000)
        {
            lastPingSendTime = curTime;

            std::string msg = "";
            // get all RTTs
            for (const auto& player : players_list)
            {
                msg += std::format("id: {}, nickname: {}, RTT: {}\n", player.id, player.nickname, player.roundTripTime());
            }
            // send RTTs to all players
            for (const auto& player : players_list)
            {
                send_unreliable(player.peer, msg.c_str());
            }
        }
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}