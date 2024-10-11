#include <enet/enet.h>
#include <iostream>
#include <vector>
#include <format>
#include "utils.h"

int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }
  ENetAddress address;

  address.host = ENET_HOST_ANY;
  address.port = 10887;

  ENetHost *lobby = enet_host_create(&address, 32, 2, 0, 0);

  if (!lobby)
  {
    printf("Cannot create ENet lobby\n");
    return 1;
  }

  enet_address_set_host(&address, "localhost");
  address.port = 10888;

  ENetPeer* serverPeer = enet_host_connect(lobby, &address, 2, 0);
  if (!serverPeer)
  {
      printf("Cannot connect to server");
      return 1;
  }

  std::vector<ENetPeer*> peers = { };

  bool gameStarted = false;
  while (true)
  {
    ENetEvent event;
    while (enet_host_service(lobby, &event, 10) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        peers.push_back(event.peer);
        if (gameStarted)
        {
            const std::string msg = std::format("server info: {}:{}", serverPeer->address.host, serverPeer->address.port);
            send_reliable(event.peer, msg.c_str());
        }
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        printf("Packet received '%s'\n", event.packet->data);
        if (!gameStarted && strcmp(reinterpret_cast<const char*>(event.packet->data), "START") == 0)
        {
            printf("Starting game...\n");
            gameStarted = true;
            const std::string msg = std::format("server info: {}:{}", serverPeer->address.host, serverPeer->address.port);
            for (const auto& peer : peers)
            {
                send_reliable(peer, msg.c_str());
            }
        }
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
  }

  enet_host_destroy(lobby);

  atexit(enet_deinitialize);
  return 0;
}

