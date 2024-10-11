#include "raylib.h"
#include <enet/enet.h>
#include <iostream>
#include <format>
#include "utils.h"

int main(int argc, const char **argv)
{
  int width = 800;
  int height = 600;
  InitWindow(width, height, "w6 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 2, 2, 0, 0);
  if (!client)
  {
    printf("Cannot create ENet client\n");
    return 1;
  }

  ENetAddress lobbyAddress;
  enet_address_set_host(&lobbyAddress, "localhost");
  lobbyAddress.port = 10887;

  ENetPeer *lobbyPeer = enet_host_connect(client, &lobbyAddress, 2, 0);
  if (!lobbyPeer)
  {
    printf("Cannot connect to lobby");
  }

  ENetPeer* serverPeer = nullptr;
  int myid = -1;
  std::string mynickname = "";
  std::string playersList = "";

  uint32_t timeStart = enet_time_get();
  uint32_t lastPosSendTime = timeStart;
  const char* data;
  bool connected = false;
  float posx = 0.f;
  float posy = 0.f;
  while (!WindowShouldClose())
  {
    const float dt = GetFrameTime();
    ENetEvent event;
    while (enet_host_service(client, &event, 10) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        connected = true;
        break;
      case ENET_EVENT_TYPE_RECEIVE:
          data = reinterpret_cast<const char*>(event.packet->data);
          printf("Packet received '%s'\n", data);

          if (match_exists(data, "your_id"))
              myid = std::stoi(find_match(data, "your_id: (\\d+)", 1));

          if (match_exists(data, "your_nickname"))
              mynickname = find_match(data, "your_nickname: (.+)", 1);

          if (match_exists(data, "server info"))
          {
              ENetAddress address;
              enet_address_set_host(&address, "localhost");
              address.port = 10888;

              serverPeer = enet_host_connect(client, &address, 2, 0);
              if (!serverPeer)
              {
                  printf("Cannot connect to server");
              }
          }

          if (match_exists(data, "RTT"))
          {
              playersList = data;
          }
        enet_packet_destroy(event.packet);
        break;
      default:
        break;
      };
    }
    if (connected)
    {
      uint32_t curTime = enet_time_get();
      if (IsKeyDown(KEY_ENTER))
      {
          send_reliable(lobbyPeer, "START");
      }
      if (serverPeer != nullptr && curTime - lastPosSendTime > 100)
      {
          lastPosSendTime = curTime;
          const std::string msg = std::format("id: {}, posx: {}, posy: {}", myid, posx, posy);
          send_unreliable(serverPeer, msg.c_str());
      }
    }
    bool left = IsKeyDown(KEY_LEFT);
    bool right = IsKeyDown(KEY_RIGHT);
    bool up = IsKeyDown(KEY_UP);
    bool down = IsKeyDown(KEY_DOWN);
    constexpr float spd = 10.f;
    posx += ((left ? -1.f : 0.f) + (right ? 1.f : 0.f)) * dt * spd;
    posy += ((up ? -1.f : 0.f) + (down ? 1.f : 0.f)) * dt * spd;

    BeginDrawing();
      ClearBackground(BLACK);
      DrawText(TextFormat("My ID: %d, My Nickname: %s", myid, mynickname.c_str()), 20, 20, 20, WHITE);
      DrawText(TextFormat("My position: (%d, %d)", (int)posx, (int)posy), 20, 40, 20, WHITE);
      DrawText(TextFormat("List of players:\n%s", playersList.c_str()), 20, 60, 20, WHITE);
    EndDrawing();
  }
  return 0;
}
