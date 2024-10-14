// initial skeleton is a clone from https://github.com/jpcy/bgfx-minimal-example
//
#include <functional>
#include "raylib.h"
#include <enet/enet.h>
#include <math.h>

#include <vector>
#include "entity.h"
#include "protocol.h"
#include <iostream>


static std::vector<Entity> entities;
static uint16_t my_entity = invalid_entity;

static std::vector<Snapshot> snapshotsHistory;

constexpr uint32_t FIXED_DT = 20;
constexpr uint32_t TIMEOUT = 100;

static uint32_t startTimeTick = 0;
static uint32_t currentTimeTick = 0;

void on_new_entity_packet(ENetPacket *packet)
{
  Entity newEntity;
  deserialize_new_entity(packet, newEntity);
  // TODO: Direct adressing, of course!
  for (const Entity &e : entities)
    if (e.eid == newEntity.eid)
      return; // don't need to do anything, we already have entity
  entities.push_back(newEntity);
  enet_packet_destroy(packet);
}

void on_set_controlled_entity(ENetPacket *packet)
{
  deserialize_set_controlled_entity(packet, my_entity);
  enet_packet_destroy(packet);
}

void on_snapshot(ENetPacket *packet)
{
  uint16_t eid = invalid_entity;
  float x = 0.f; float y = 0.f; float ori = 0.f; uint32_t timeTick = 0;
  deserialize_snapshot(packet, eid, x, y, ori, timeTick);
  // TODO: Direct adressing, of course!
  for (Entity& e : entities)
  {
      if (e.eid == eid)
      {
          if (e.eid == my_entity)
          {
              snapshotsHistory.push_back(Snapshot(x, y, ori, timeTick));
              if (snapshotsHistory.size() > 3)
              {
                  snapshotsHistory.erase(snapshotsHistory.begin());
              }
          }
          else
          {
              e.x = x;
              e.y = y;
              e.ori = ori;
          }
      }

  }
  enet_packet_destroy(packet);
}
void on_time_sync(ENetEvent& event)
{
    uint32_t time;
    deserialize_time(event.packet, time);
    enet_time_set(time + event.peer->roundTripTime / 2);
    currentTimeTick = time / FIXED_DT;
    startTimeTick = currentTimeTick;
}
float lagrange_interpolation(double currentTime, double t0, double t1, double t2, double v0, double v1, double v2)
{
    double l0 = (currentTime - t1) * (currentTime - t2) / ((t0 - t1) * (t0 - t2));
    double l1 = (currentTime - t0) * (currentTime - t2) / ((t1 - t0) * (t1 - t2));
    double l2 = (currentTime - t0) * (currentTime - t1) / ((t2 - t0) * (t2 - t1));
    return  v0 * l0 + v1 * l1 + v2 * l2;
}
void interpolate(Entity& e, uint32_t timeTick)
{
    if (snapshotsHistory.size() == 3)
    {
        if (snapshotsHistory[0].timeTick == snapshotsHistory[2].timeTick)
        {
            return;
        }
        e.x = lagrange_interpolation(timeTick, snapshotsHistory[0].timeTick, snapshotsHistory[1].timeTick, snapshotsHistory[2].timeTick,
            snapshotsHistory[0].x, snapshotsHistory[1].x, snapshotsHistory[2].x);
        e.y = lagrange_interpolation(timeTick, snapshotsHistory[0].timeTick, snapshotsHistory[1].timeTick, snapshotsHistory[2].timeTick,
            snapshotsHistory[0].y, snapshotsHistory[1].y, snapshotsHistory[2].y);
        e.ori = lagrange_interpolation(timeTick, snapshotsHistory[0].timeTick, snapshotsHistory[1].timeTick, snapshotsHistory[2].timeTick,
            snapshotsHistory[0].ori, snapshotsHistory[1].ori, snapshotsHistory[2].ori);
    }
}
void simulate(Entity& e, float dt, float thr, float steer)
{
    e.thr = thr;
    e.steer = steer;
    simulate_entity(e, dt);
}
int main(int argc, const char **argv)
{
  if (enet_initialize() != 0)
  {
    printf("Cannot init ENet");
    return 1;
  }

  ENetHost *client = enet_host_create(nullptr, 1, 2, 0, 0);
  if (!client)
  {
    printf("Cannot create ENet client\n");
    return 1;
  }

  ENetAddress address;
  enet_address_set_host(&address, "localhost");
  address.port = 10131;

  ENetPeer *serverPeer = enet_host_connect(client, &address, 2, 0);
  if (!serverPeer)
  {
    printf("Cannot connect to server");
    return 1;
  }

  int width = 600;
  int height = 600;

  InitWindow(width, height, "w5 networked MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.rotation = 0.f;
  camera.zoom = 10.f;


  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second

  bool connected = false;
  while (!WindowShouldClose())
  {
    float dt = GetFrameTime();
    ENetEvent event;
    while (enet_host_service(client, &event, 0) > 0)
    {
      switch (event.type)
      {
      case ENET_EVENT_TYPE_CONNECT:
        printf("Connection with %x:%u established\n", event.peer->address.host, event.peer->address.port);
        send_join(serverPeer);
        connected = true;
        break;
      case ENET_EVENT_TYPE_RECEIVE:
        switch (get_packet_type(event.packet))
        {
        case E_SERVER_TO_CLIENT_NEW_ENTITY:
          on_new_entity_packet(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SET_CONTROLLED_ENTITY:
          on_set_controlled_entity(event.packet);
          break;
        case E_SERVER_TO_CLIENT_SNAPSHOT:
          on_snapshot(event.packet);
          break;
        };
      case E_SERVER_TO_CLIENT_TIME_SYNC:
          on_time_sync(event);
          break;
        break;
      default:
        break;
      };
    }

    uint32_t currentTime = enet_time_get();

    if (my_entity != invalid_entity)
    {
      bool left = IsKeyDown(KEY_LEFT);
      bool right = IsKeyDown(KEY_RIGHT);
      bool up = IsKeyDown(KEY_UP);
      bool down = IsKeyDown(KEY_DOWN);
      // TODO: Direct adressing, of course!
      for (Entity& e : entities)
      {
          if (e.eid == my_entity)
          {
              // Update
              float thr = (up ? 1.f : 0.f) + (down ? -1.f : 0.f);
              float steer = (left ? -1.f : 0.f) + (right ? 1.f : 0.f);

              send_entity_input(serverPeer, my_entity, thr, steer, currentTime / FIXED_DT);
              simulate(e, dt, thr, steer);
              interpolate(e, (currentTime - TIMEOUT) / FIXED_DT);
          }
      }
    }

    BeginDrawing();
      ClearBackground(GRAY);
      BeginMode2D(camera);
        for (const Entity &e : entities)
        {
          const Rectangle rect = {e.x, e.y, 3.f, 1.f};
          DrawRectanglePro(rect, {0.f, 0.5f}, e.ori * 180.f / PI, GetColor(e.color));
        }

      EndMode2D();
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
